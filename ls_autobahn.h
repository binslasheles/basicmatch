#ifndef __LS_AUTOBAHN_H__
#define __LS_AUTOBAHN_H__

#include <msgpack.hpp>
#include <string>
#include "st_socket.h"
#include "delegate.h"
#include <map>
#include <memory>
#include <unordered_map>
#include <functional>
#include <iostream>

namespace Autobahn {

struct buffer {

    buffer(uint32_t bytes) : bufsize_(bytes + sizeof(uint32_t)), buf_((char*)::malloc(bufsize_)) { clear(); }

    ~buffer() { ::free(buf_); }

    inline uint32_t& header()      { return *(uint32_t*)buf_; }
    inline uint8_t*  serial_ptr()  { return (uint8_t*)buf_; }

    inline uint32_t  serial_size() { return in_ - buf_; }
    inline uint32_t  data_size()   { return serial_size() - sizeof(uint32_t); }

    inline void clear() { in_ = buf_ + sizeof(uint32_t); }

    inline void write(const char* data, std::size_t bytes) {
        if(in_ + bytes > buf_ + bufsize_) {
            int off  = in_ - buf_;
            bufsize_ += (bytes  + 128); //XXX
            buf_     = (char*)realloc(buf_, bufsize_);
            in_      = buf_ + off;
        }

        memcpy(in_, data, bytes);
        in_ += bytes;
    }

private:
    uint32_t bufsize_;
    char* buf_;
    char* in_;
};

typedef msgpack::object object;
typedef msgpack::zone objbuf;

struct session {
    typedef delegate<object(const std::vector<object>&, const std::map<std::string, object>&)>endpoint_t;
    typedef delegate<std::vector<object>(const std::vector<object>&, const std::map<std::string, object>&)> endpoint_v_t;
    typedef delegate<std::map<std::string, object>(const std::vector<object>&, const std::map<std::string, object>&)>endpoint_m_t;
    typedef delegate<std::pair<std::vector<object>, std::map<std::string, object>>(const std::vector<object>&, const std::map<std::string, object>&)> endpoint_vm_t;
    typedef delegate<void(uint64_t)> join_cb_t;
    typedef delegate<void(const std::vector<object>&, const std::map<std::string, object>&)> subscribed_cb_t;
    typedef delegate<void(const object&)> called_cb_t;
    typedef delegate<void(const std::string&)> session_end_cb_t;

    session(const std::string& remote_addr, const std::string& realm, const join_cb_t& jcb, const session_end_cb_t& secb);

    void stop();
    void leave(const std::string& reason = std::string("wamp.error.close_realm"));

    void subscribe(const std::string& topic, const subscribed_cb_t& handler);

    void call(const std::string& remote,
              const called_cb_t& cb);

    void call(const std::string& remote,
              const std::vector<object>&,
              const called_cb_t& cb);

    void call(const std::string& remote,
              const std::vector<object>&,
              const std::map<std::string, object>&,
              const called_cb_t& cb);


    void provide(const std::string& procedure, const endpoint_t&   endpoint);
    void provide(const std::string& procedure, const endpoint_v_t&  endpoint);
    void provide(const std::string& procedure, const endpoint_m_t&  endpoint);
    void provide(const std::string& procedure, const endpoint_vm_t& endpoint);

    void publish(const std::string& topic);
    void publish(const std::string&, const std::vector<object>&);
    void publish(const std::string&, const std::vector<object>&, const std::map<std::string,object >&);

private:

     enum class msg_code : int {
        HELLO        = 1,
        WELCOME      = 2,
        ABORT        = 3,
        CHALLENGE    = 4,
        AUTHENTICATE = 5,
        GOODBYE      = 6,
        HEARTBEAT    = 7,
        ERROR        = 8,
        PUBLISH      = 16,
        PUBLISHED    = 17,
        SUBSCRIBE    = 32,
        SUBSCRIBED   = 33,
        UNSUBSCRIBE  = 34,
        UNSUBSCRIBED = 35,
        EVENT        = 36,
        CALL         = 48,
        CANCEL       = 49,
        RESULT       = 50,
        REGISTER     = 64,
        REGISTERED   = 65,
        UNREGISTER   = 66,
        UNREGISTERED = 67,
        INVOCATION   = 68,
        INTERRUPT    = 69,
        YIELD        = 70
     };

    uint64_t next_request_id() { return request_id_++; }

    typedef std::vector<object> wamp_msg_t;

    void     send();
    void     parse_msg(uint8_t * buf, uint32_t msg_size);
    void     handle_msg(const object& obj);
    void     subscribed(const wamp_msg_t& msg);
    void     event(const wamp_msg_t& msg);
    void     goodbye(const wamp_msg_t& msg);
    void     call_result(const wamp_msg_t& msg);
    void     registered(const wamp_msg_t& msg);
    void     invoked(const wamp_msg_t& msg);
    void     error(const wamp_msg_t& msg);

    template<typename Ept>
    void provide_impl(const std::string& procedure, const Ept& endpoint);

    void join();

    bool prepare_call(const std::string& rp, uint32_t argc, const called_cb_t& cb);
    bool prepare_publish(const std::string& topic, uint32_t addl);
    void clear();

    std::unordered_map<uint64_t, subscribed_cb_t> pending_subscriptions_; //request_id => sub_data
    std::unordered_map<uint64_t, subscribed_cb_t> subscriptions_; //sub => sub_data

    std::unordered_map<uint64_t, called_cb_t> calls_; //request_id => sub_data

    std::unordered_map<uint64_t, std::unique_ptr<delegate_base>> pending_procs_;
    std::unordered_map<uint64_t, std::unique_ptr<delegate_base>> procs_;

    static void recv_cb(void * user_data, uint8_t * buf, uint16_t& bytes);
    static void error_cb(void * user_data, int error);
    buffer    buf_;
    msgpack::packer<buffer> packer_;

    join_cb_t jcb_;
    session_end_cb_t secb_;


    TcpSocket   sock_;
    std::string realm_;
    uint64_t session_id_ = 0;
    uint64_t request_id_ = 0;
    bool     stopped_    = false;
    bool     leaving_    = false;
};


struct protocol_error : public std::runtime_error {
    protocol_error(const std::string& msg) : std::runtime_error(msg) {};
};

struct no_session_error : public std::runtime_error {
    no_session_error() : std::runtime_error("session not joined") {};
};

};

#endif
