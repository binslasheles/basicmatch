///////////////////////////////////////////////////////////////////////////////
//
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
//  Tavendo GmbH
///////////////////////////////////////////////////////////////////////////////

#include "ls_autobahn.h"
#include <utility>
#include <unistd.h>

namespace Autobahn {

session::session(const std::string& remote_addr,  const std::string& realm, const join_cb_t& jcb, const session_end_cb_t& secb)
    : buf_(256), packer_(&buf_), jcb_(jcb), secb_(secb), sock_(SocketAddr(), SocketAddr(remote_addr)), realm_(realm) {

    sock_.set_recv_cb(this, Autobahn::session::recv_cb);
    sock_.set_error_cb(Autobahn::session::error_cb);

    if( sock_.connect() )
        join();
    else
        std::cerr << "ERROR: failed to connect to <" << remote_addr << ">. session not started" << std::endl;
}

void session::join() {
    packer_.pack_array(3);

    packer_.pack(static_cast<int> (msg_code::HELLO));
    packer_.pack(realm_);

    packer_.pack_map(1);
    packer_.pack(std::string("roles"));

    packer_.pack_map(4);

    packer_.pack(std::string("caller"));
    packer_.pack_map(0);

    packer_.pack(std::string("callee"));
    packer_.pack_map(0);

    packer_.pack(std::string("publisher"));
    packer_.pack_map(0);

    packer_.pack(std::string("subscriber"));
    packer_.pack_map(0);

    send();
}

void session::leave(const std::string& why) {
    if (!session_id_) {
        throw no_session_error();
    }

    leaving_    = true;
    session_id_ = 0;

    // [GOODBYE, Details|dict, Reason|uri]

    packer_.pack_array(3);

    packer_.pack(static_cast<int> (msg_code::GOODBYE));
    packer_.pack_map(0);
    packer_.pack(why);
    send();
}

void session::clear() {
    session_id_ = 0;
    pending_subscriptions_.clear();
    subscriptions_.clear();
    calls_.clear();
    pending_procs_.clear();
    procs_.clear();
}

void session::goodbye(const wamp_msg_t& msg) {
    if (!leaving_) {
        // if we did not initiate closing, reply ..

        // [GOODBYE, Details|dict, Reason|uri]

        packer_.pack_array(3);

        packer_.pack(static_cast<int> (msg_code::GOODBYE));
        packer_.pack_map(0);
        packer_.pack(std::string("wamp.error.goodbye_and_out"));
        send();

    }

    std::string txt(msg[2].as<std::string>());
    secb_(txt);
    clear();
}


void session::error_cb(void * user_data, int error) {
    if( !error ) {
        std::cerr << "==== SOCKET CLOSED =====" << std::endl;
    }
}

void session::recv_cb(void * user_data, uint8_t * buf, uint16_t & bytes) {
    //std::cerr << "==== got <" <<  bytes << "> bytes ==== " <<std::endl;
    session& s = *(session*)user_data;

    uint32_t msg_size = 0;

    while( !s.stopped_ && bytes >= sizeof(msg_size) ) {
        msg_size = ntohl(*(uint32_t*)buf);

        if( bytes < msg_size ) {
            break;
        } else {
            s.parse_msg(buf + sizeof(msg_size), msg_size);

            msg_size += sizeof(msg_size);
            buf   += msg_size;
            bytes -= msg_size;
        }
    }

    if(s.stopped_)
        bytes = 0;
}

void session::subscribed(const wamp_msg_t& msg) {
    if(leaving_)
        return;
    // [SUBSCRIBED, SUBSCRIBE.Request|id, Subscription|id]

    if (msg.size() != 3) {
        throw protocol_error("invalid SUBSCRIBED message structure - length must be 3");
    }

    if (msg[1].type != msgpack::type::POSITIVE_INTEGER) {
        throw protocol_error("invalid SUBSCRIBED message structure - SUBSCRIBED.Request must be an integer");
    }

    uint64_t request_id = msg[1].as<uint64_t>();

    auto it = pending_subscriptions_.find(request_id);

    if (it == pending_subscriptions_.end()) {
        throw protocol_error("bogus SUBSCRIBED message for non-pending request ID");
    } else {

        if (msg[2].type != msgpack::type::POSITIVE_INTEGER) {
            throw protocol_error("invalid SUBSCRIBED message structure - SUBSCRIBED.Subscription must be an integer");
        }

        uint64_t subscription_id = msg[2].as<uint64_t>();

        subscriptions_.emplace(std::piecewise_construct,
            std::forward_as_tuple(subscription_id),
            std::forward_as_tuple(it->second)
        );

        pending_subscriptions_.erase(request_id);
    }
}

void session::event(const wamp_msg_t& msg) {
    if(leaving_)
        return;
    // [EVENT, SUBSCRIBED.Subscription|id, PUBLISHED.Publication|id, Details|dict]
    // [EVENT, SUBSCRIBED.Subscription|id, PUBLISHED.Publication|id, Details|dict, PUBLISH.Arguments|list]
    // [EVENT, SUBSCRIBED.Subscription|id, PUBLISHED.Publication|id, Details|dict, PUBLISH.Arguments|list, PUBLISH.ArgumentsKw|dict]

    if (msg.size() != 4 && msg.size() != 5 && msg.size() != 6) {
        throw protocol_error("invalid EVENT message structure - length must be 4, 5 or 6");
    }

    if (msg[1].type != msgpack::type::POSITIVE_INTEGER) {
        throw protocol_error("invalid EVENT message structure - SUBSCRIBED.Subscription must be an integer");
    }

    uint64_t subscription_id = msg[1].as<uint64_t>();

    auto range = subscriptions_.equal_range(subscription_id);
    //auto handlersEnd = m_handlers.upper_bound(subscription_id);
    //if (handlersBegin != m_handlers.end() && handlersBegin != handlersEnd) {
    if(range.first == range.second) {
        std::cerr << "Skipping EVENT for non-existent subscription ID " << subscription_id << std::endl;
    } else {

        if (msg[2].type != msgpack::type::POSITIVE_INTEGER) {
            throw protocol_error("invalid EVENT message structure - PUBLISHED.Publication|id must be an integer");
        }

        if (msg[3].type != msgpack::type::MAP) {
            throw protocol_error("invalid EVENT message structure - Details must be a dictionary");
        }

        std::vector<object> raw_args;
        std::map<std::string, object> raw_kwargs;
        //anyvec args; XXX
        //anymap kwargs; XXX
        if (msg.size() > 4) {

            if (msg[4].type != msgpack::type::ARRAY) {
                throw protocol_error("invalid EVENT message structure - EVENT.Arguments must be a list");
            }

            msg[4].convert(&raw_args);

            if (msg.size() > 5) {

                if (msg[5].type != msgpack::type::MAP) {
                    throw protocol_error("invalid EVENT message structure - EVENT.Arguments must be a list");
                }

                msg[5].convert(&raw_kwargs);
            }
        }

        try {
            // now trigger the user supplied event handler ..
            //
            for(; range.first != range.second; ++range.first) {
            //while (range.first != range.second) {
                (range.first->second)(raw_args, raw_kwargs);
            }

        } catch (...) {
            std::cerr << "Warning: event handler fired exception" << std::endl;
        }
    }
}

void session::error(const wamp_msg_t& msg) {

    // [ERROR, REQUEST.Type|int, REQUEST.Request|id, Details|dict, Error|uri]
    // [ERROR, REQUEST.Type|int, REQUEST.Request|id, Details|dict, Error|uri, Arguments|list]
    // [ERROR, REQUEST.Type|int, REQUEST.Request|id, Details|dict, Error|uri, Arguments|list, ArgumentsKw|dict]

    // message length
    //
    if (msg.size() != 5 && msg.size() != 6 && msg.size() != 7) {
        throw protocol_error("invalid ERROR message structure - length must be 5, 6 or 7");
    }

    // REQUEST.Type|int
    //
    if (msg[1].type != msgpack::type::POSITIVE_INTEGER) {
        throw protocol_error("invalid ERROR message structure - REQUEST.Type must be an integer");
    }

    msg_code request_type = static_cast<msg_code> (msg[1].as<int>());

    if (request_type != msg_code::CALL &&
        request_type != msg_code::REGISTER &&
        request_type != msg_code::UNREGISTER &&
        request_type != msg_code::PUBLISH &&
        request_type != msg_code::SUBSCRIBE &&
        request_type != msg_code::UNSUBSCRIBE) {
        throw protocol_error("invalid ERROR message - ERROR.Type must one of CALL, REGISTER, UNREGISTER, SUBSCRIBE, UNSUBSCRIBE");
    }

    // REQUEST.Request|id
    //
    if (msg[2].type != msgpack::type::POSITIVE_INTEGER) {
        throw protocol_error("invalid ERROR message structure - REQUEST.Request must be an integer");
    }

    uint64_t request_id = msg[2].as<uint64_t>();

    // Details
    //
    if (msg[3].type != msgpack::type::MAP) {
        throw protocol_error("invalid ERROR message structure - Details must be a dictionary");
    }

    // Error|uri
    //
    if (msg[4].type != msgpack::type::STR) {
        throw protocol_error("invalid ERROR message - Error must be a string (URI)");
    }

    std::string error = msg[4].as<std::string>();

    // Arguments|list
    //
    if (msg.size() > 5) {
        if (msg[5].type  != msgpack::type::ARRAY) {
            throw protocol_error("invalid ERROR message structure - Arguments must be a list");
        }
    }

    // ArgumentsKw|list
    //
    if (msg.size() > 6) {
        if (msg[6].type  != msgpack::type::MAP) {
            throw protocol_error("invalid ERROR message structure - ArgumentsKw must be a dictionary");
        }
    }

    switch (request_type) {

    case msg_code::CALL:
    {
        auto it = calls_.find(request_id);

        if (it != calls_.end()) {

            std::cerr << "ERROR: request <" << request_id << "> text <" << error << ">" << std::endl;
        } else {
            throw protocol_error("bogus ERROR message for non-pending CALL request ID");
        }
    } break;


    // FIXME: handle other error messages
    default:
        std::cerr << "unhandled ERROR message" << std::endl;
    }
}



void session::handle_msg(const object& obj) {
    //std::cerr << "================ GOT MESSAGE =============" << std::endl;

    if (obj.type != msgpack::type::ARRAY) {
        throw protocol_error("invalid message structure - message is not an array");
    }

    wamp_msg_t msg;
    obj.convert(&msg);

    if (msg.size() < 1) {
        throw protocol_error("invalid message structure - missing message code");
    }

    if (msg[0].type != msgpack::type::POSITIVE_INTEGER) {
        throw protocol_error("invalid message code type - not an integer");
    }

    msg_code code = static_cast<msg_code> (msg[0].as<int>());

    switch (code) {
    case msg_code::HELLO:
        throw protocol_error("received HELLO message unexpected for WAMP client roles");
        break;

    case msg_code::WELCOME:
        //used to be process welcome
        session_id_ = msg[1].as<uint64_t>();

        if(jcb_)
            jcb_(msg[1].as<uint64_t>());

        break;

    case msg_code::ABORT:
        // FIXME
        break;

    case msg_code::CHALLENGE:
        throw protocol_error("received CHALLENGE message - not implemented");
        break;

    case msg_code::AUTHENTICATE:
        throw protocol_error("received AUTHENTICATE message unexpected for WAMP client roles");
        break;

    case msg_code::GOODBYE:
        goodbye(msg);
        break;

    case msg_code::HEARTBEAT:
        break;

    case msg_code::ERROR:
        error(msg);
        break;

    case msg_code::PUBLISH:
        throw protocol_error("received PUBLISH message unexpected for WAMP client roles");
        break;

    case msg_code::PUBLISHED:
        // FIXME
        break;

    case msg_code::SUBSCRIBE:
        throw protocol_error("received SUBSCRIBE message unexpected for WAMP client roles");
        break;

    case msg_code::SUBSCRIBED:
        subscribed(msg);
        break;

    case msg_code::UNSUBSCRIBE:
        throw protocol_error("received UNSUBSCRIBE message unexpected for WAMP client roles");
        break;

    case msg_code::UNSUBSCRIBED:
    // FIXME
        break;

    case msg_code::EVENT:
        event(msg);
        break;

    case msg_code::CALL:
        throw protocol_error("received CALL message unexpected for WAMP client roles");
        break;

    case msg_code::CANCEL:
        throw protocol_error("received CANCEL message unexpected for WAMP client roles");
        break;

    case msg_code::RESULT:
        call_result(msg);
        break;

    case msg_code::REGISTER:
        throw protocol_error("received REGISTER message unexpected for WAMP client roles");
        break;

    case msg_code::REGISTERED:
        registered(msg);
        break;

    case msg_code::UNREGISTER:
        throw protocol_error("received UNREGISTER message unexpected for WAMP client roles");
        break;

    case msg_code::UNREGISTERED:
        // FIXME
        break;

    case msg_code::INVOCATION:
        invoked(msg);
        break;

    case msg_code::INTERRUPT:
        throw protocol_error("received INTERRUPT message - not implemented");
        break;

    case msg_code::YIELD:
        throw protocol_error("received YIELD message unexpected for WAMP client roles");
        break;
    }
}

void session::parse_msg(uint8_t* buf, uint32_t msg_size) {
    //if (m_debug) {
    //   std::cerr << "RX message received." << std::endl;
    //}

    //std::cerr << "================ parse msg ===============" << std::endl;
    std::size_t off = 0;

    while(off != msg_size) {
        msgpack::unpacked result;

        msgpack::unpack(&result, (const char*)buf, msg_size, &off);
        object obj(result.get());

        handle_msg(obj);
    }
}

void session::stop() {
    stopped_ = true;
}

//could templatize
bool session::prepare_call(const std::string& rp, uint32_t addl, const called_cb_t& cb) {
    if( !session_id_ ) {
        std::cerr << "error: no session id" << std::endl;
        return false;
    }

    if(addl > 2) {
        std::cerr << "argc must less than or equal to 2" << std::endl;
        return false;
    }

    uint64_t request_id = next_request_id();

    calls_.emplace(std::piecewise_construct,
        std::forward_as_tuple(request_id),
        std::forward_as_tuple(cb)
    );

    // [CALL, Request|id, Options|dict, Procedure|uri]

    packer_.pack_array(4 + addl);
    packer_.pack(static_cast<int> (msg_code::CALL));
    packer_.pack(request_id);
    packer_.pack_map(0);
    packer_.pack(rp);
    return true;
}

inline
void pack_vec(msgpack::packer<buffer>& packer, const std::vector<object>& vecargs ) {

    packer.pack_array(vecargs.size());

    for(auto& obj : vecargs)
        packer.pack(obj);
}

inline
void pack_map(msgpack::packer<buffer>& packer, const std::map<std::string, object>& mapargs ) {

    packer.pack_map(mapargs.size());

    for(auto& apair : mapargs)
    {
        packer.pack(apair.first);
        packer.pack(apair.second);
    }
}

void session::call(const std::string& rp, const called_cb_t& cb) {
    if( prepare_call(rp, 0, cb) )
        send();
}

void session::call(const std::string& rp, const std::vector<object>& vecargs, const called_cb_t& cb) {
    if( !vecargs.size() )
        call(rp, cb);
    else if( prepare_call(rp, 1, cb) ) {

        pack_vec(packer_, vecargs);

        send();
    }
}

void session::call(const std::string& rp, const std::vector<object>& vecargs, const std::map<std::string, object>& mapargs,
    const called_cb_t& cb) {
    if( !mapargs.size() )
        call(rp, vecargs, cb);
    else if( prepare_call(rp, 2, cb) ) {

        pack_vec(packer_, vecargs);
        pack_map(packer_, mapargs);

        send();
    }

}

bool session::prepare_publish(const std::string& topic, uint32_t addl) {
    if( !session_id_ ) {
        std::cerr << "error: no session id" << std::endl;
        return false;
    }

    if(addl > 2) {
        std::cerr << "argc less than or equal to 2" << std::endl;
        return false;
    }

    uint64_t request_id = next_request_id();

    // [PUBLISH, Request|id, Options|dict, Topic|uri]

    packer_.pack_array(4 + addl);
    packer_.pack(static_cast<int> (msg_code::PUBLISH));
    packer_.pack(request_id);
    packer_.pack_map(0);
    packer_.pack(topic);
    return true;
}

void session::publish(const std::string& topic) {
    if( prepare_publish(topic, 0) )
        send();
}

void session::publish(const std::string& topic, const std::vector<object>& vecargs) {
    if( !vecargs.size() )
        publish(topic);
    else if (prepare_publish(topic, 1) ) {
        pack_vec(packer_, vecargs);

        send();
    }
}

void session::publish(const std::string& topic, const std::vector<object>& vecargs, const std::map<std::string,object >& mapargs) {
    if(!mapargs.size())
        publish(topic, vecargs);
    else if( prepare_publish(topic, 2) ) {
        pack_vec(packer_, vecargs);
        pack_map(packer_, mapargs);
        send();
    }
}

void session::call_result(const wamp_msg_t& msg) {
    if(leaving_)
        return;

    // [RESULT, CALL.Request|id, Details|dict]
    // [RESULT, CALL.Request|id, Details|dict, YIELD.Arguments|list]
    // [RESULT, CALL.Request|id, Details|dict, YIELD.Arguments|list, YIELD.ArgumentsKw|dict]

    if (msg.size() != 3 && msg.size() != 4 && msg.size() != 5) {
        throw protocol_error("invalid RESULT message structure - length must be 3, 4 or 5");
    }

    if (msg[1].type != msgpack::type::POSITIVE_INTEGER) {
        throw protocol_error("invalid RESULT message structure - CALL.Request must be an integer");
    }

    uint64_t request_id = msg[1].as<uint64_t>();

    auto it = calls_.find(request_id);

    if ( it == calls_.end()) {
        throw protocol_error("bogus RESULT message for non-pending request ID");
    } else {

        if (msg[2].type != msgpack::type::MAP) {
            throw protocol_error("invalid RESULT message structure - Details must be a dictionary");
        }

        if (msg.size() > 3) {

            if (msg[3].type != msgpack::type::ARRAY) {
                throw protocol_error("invalid RESULT message structure - YIELD.Arguments must be a list");
            }

            std::vector<msgpack::object> raw_args;
            msg[3].convert(&raw_args);

            //anyvec args;

            //unpack_anyvec(raw_args, args);

            if (raw_args.size() > 0) {
                //std::vector<msgpack::object> args;
                //raw_args[0].convert(&args);

                (it->second)(raw_args[0]);
                //call->second.m_res.set_value(args[0]);
            } else {
                (it->second)(object());
                //call->second.m_res.set_value(boost::any());
            }

        } else {
            // empty result
            (it->second)(object());
        }
    }
}

void session::subscribe(const std::string& topic, const subscribed_cb_t& cb) {
    if( !session_id_ ) {
        std::cerr << "error: no session id" << std::endl;
        return;
    }

    uint64_t request_id = next_request_id();

    pending_subscriptions_.emplace(std::piecewise_construct,
        std::forward_as_tuple(request_id),
        std::forward_as_tuple(cb)
    );

    packer_.pack_array(4);
    packer_.pack(static_cast<int> (msg_code::SUBSCRIBE));
    packer_.pack(request_id);
    packer_.pack_map(0);
    packer_.pack(topic);
    send();
}


template<typename Ept>
inline void session::provide_impl(const std::string& procedure, const Ept& endpoint) {
    if (!session_id_) {
        throw no_session_error();
    }

    uint64_t request_id = next_request_id();

    std::unique_ptr<delegate_base> handler(new Ept(endpoint));

    pending_procs_.emplace(std::piecewise_construct,
        std::forward_as_tuple(request_id),
        std::forward_as_tuple(std::move(handler))
    );

    // [REGISTER, Request|id, Options|dict, Procedure|uri]

    packer_.pack_array(4);
    packer_.pack(static_cast<int> (msg_code::REGISTER));
    packer_.pack(request_id);
    packer_.pack_map(0);
    packer_.pack(procedure);
    send();
}

void session::provide(const std::string& procedure, const endpoint_t&   endpoint)  {
    provide_impl(procedure, endpoint);
}

void session::provide(const std::string& procedure, const endpoint_v_t&  endpoint) {
    provide_impl(procedure, endpoint);
}

void session::provide(const std::string& procedure, const endpoint_m_t&  endpoint) {
    provide_impl(procedure, endpoint);
}

void session::provide(const std::string& procedure, const endpoint_vm_t& endpoint) {
    provide_impl(procedure, endpoint);
}

void session::registered(const wamp_msg_t& msg) {
    if(leaving_)
        return;
// [REGISTERED, REGISTER.Request|id, Registration|id]

    if (msg.size() != 3) {
        throw protocol_error("invalid REGISTERED message structure - length must be 3");
    }

    if (msg[1].type != msgpack::type::POSITIVE_INTEGER) {
        throw protocol_error("invalid REGISTERED message structure - REGISTERED.Request must be an integer");
    }

    uint64_t request_id = msg[1].as<uint64_t>();

    auto it = pending_procs_.find(request_id);

    if (it == pending_procs_.end()) {
        throw protocol_error("bogus REGISTERED message for non-pending request ID");
    } else {

        if (msg[2].type != msgpack::type::POSITIVE_INTEGER) {
            throw protocol_error("invalid REGISTERED message structure - REGISTERED.Registration must be an integer");
        }

        uint64_t registration_id = msg[2].as<uint64_t>();

        procs_[registration_id] = std::move(it->second);//register_request->second.m_endpoint;

        pending_procs_.erase(it);
    }
}

void session::invoked(const wamp_msg_t& msg) {

    // [INVOCATION, Request|id, REGISTERED.Registration|id, Details|dict]
    // [INVOCATION, Request|id, REGISTERED.Registration|id, Details|dict, CALL.Arguments|list]
    // [INVOCATION, Request|id, REGISTERED.Registration|id, Details|dict, CALL.Arguments|list, CALL.ArgumentsKw|dict]

    if (msg.size() != 4 && msg.size() != 5 && msg.size() != 6) {
        throw protocol_error("invalid INVOCATION message structure - length must be 4, 5 or 6");
    }

    if (msg[1].type != msgpack::type::POSITIVE_INTEGER) {
        throw protocol_error("invalid INVOCATION message structure - INVOCATION.Request must be an integer");
    }

    uint64_t request_id = msg[1].as<uint64_t>();

    if (msg[2].type != msgpack::type::POSITIVE_INTEGER) {
        throw protocol_error("invalid INVOCATION message structure - INVOCATION.Registration must be an integer");
    }

    uint64_t registration_id = msg[2].as<uint64_t>();

    auto it = procs_.find(registration_id);

    if ( it == procs_.end()) {
        throw protocol_error("bogus INVOCATION message for non-registered registration ID");
    } else {

        if (msg[3].type != msgpack::type::MAP) {
            throw protocol_error("invalid INVOCATION message structure - Details must be a dictionary");
        }

        std::vector<msgpack::object> raw_args;
        std::map<std::string, msgpack::object> raw_kwargs;

        if (msg.size() > 4) {

            if (msg[4].type != msgpack::type::ARRAY) {
                throw protocol_error("invalid INVOCATION message structure - INVOCATION.Arguments must be a list");
            }

            msg[4].convert(&raw_args);

            if (msg.size() > 5) {
                msg[5].convert(&raw_kwargs);
            }
        }

        // [YIELD, INVOCATION.Request|id, Options|dict]
        // [YIELD, INVOCATION.Request|id, Options|dict, Arguments|list]
        // [YIELD, INVOCATION.Request|id, Options|dict, Arguments|list, ArgumentsKw|dict]
        try {
            delegate_base* delegate_base = it->second.get();

            if (typeid(*delegate_base) == typeid(endpoint_t)) {

               //if (m_debug) {
               //   std::cerr << "Invoking endpoint registered under " << registration_id << " as of type endpoint_t" << std::endl;
               //}


               packer_.pack_array(4);
               packer_.pack(static_cast<int> (msg_code::YIELD));
               packer_.pack(request_id);
               packer_.pack_map(0);
               packer_.pack_array(1);
               packer_.pack(
                   (*(endpoint_t*)delegate_base)(raw_args, raw_kwargs)
               );
               send();

            } else if( typeid(*delegate_base) == typeid(endpoint_v_t)) {

               //if (m_debug) {
               //   std::cerr << "Invoking endpoint registered under " << registration_id << " as of type endpoint_v_t" << std::endl;
               //}

               //anyvec res = ( boost::any_cast<endpoint_v_t>(endpoint->second) )(args, kwargs);

               packer_.pack_array(4);
               packer_.pack(static_cast<int> (msg_code::YIELD));
               packer_.pack(request_id);
               packer_.pack_map(0);
               pack_vec(packer_, (*(endpoint_v_t*)delegate_base)(raw_args, raw_kwargs));
               send();

            } else if( typeid(*delegate_base) == typeid(endpoint_m_t)) {


                packer_.pack_array(5);
                packer_.pack(static_cast<int> (msg_code::YIELD));
                packer_.pack(request_id);
                packer_.pack_map(0);
                packer_.pack_array(0);
                pack_map(packer_, (*(endpoint_m_t*)delegate_base)(raw_args, raw_kwargs));
                send();

            } else if( typeid(*delegate_base) == typeid(endpoint_vm_t)) {

               //if (m_debug) {
               //   std::cerr << "Invoking endpoint registered under " << registration_id << " as of type endpoint_fvm_t" << std::endl;
               //}

                packer_.pack_array(5);
                packer_.pack(static_cast<int> (msg_code::YIELD));
                packer_.pack(request_id);
                packer_.pack_map(0);

                auto vm = (*(endpoint_vm_t*)delegate_base)(raw_args, raw_kwargs) ;

                pack_vec(packer_, vm.first);
                pack_map(packer_, vm.second);
                send();

            } else {
               // FIXME
               std::cerr << "FIX ME INVOCATION " << std::endl;
               //std::cerr << typeid(endpoint_t).name() << std::endl;
               //std::cerr << ((endpoint->second).type()).name() << std::endl;
            }

        }
        catch (...) {
            // FIXME: send ERROR
            std::cerr << "INVOCATION failed" << std::endl;
        }
    }
}


void session::send() {
    if(stopped_) {
        std::cerr << "send after session stopped" << std::endl;
    } else {
        // write message length prefix XXX
        uint32_t& len = buf_.header();
        len           = htonl(buf_.data_size());

        sock_.send(buf_.serial_ptr(), buf_.serial_size());

        //if (m_debug) {
        //   std::cerr << "TX message sent (" << written << " / " << (sizeof(len) + m_buffer.size()) << " octets)" << std::endl;
        //}
    }

    buf_.clear();
}

};
