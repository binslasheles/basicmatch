// Stub implementation and example driver for BasicMatch.
// Your crossing logic should be accesible from the BasicMatch class.
// Other than the signature of BasicMatch::action() you are free to modify as needed.
#include <string>
#include <memory>
#include <fstream>
#include <iostream>
#include <list>
#include "types.h"
#include "serializer.h"
#include "engine.h"
#include "st_socket.h"
#include "msg.h"

/* BasicMatch is the driver for decoupled
 * engine/matching logic and raw input deserialization.
 * support for multiple input formats (possibly simultaneously)
 * only requires more/different serializers
 *
 *
 * in all classes, minimize explicit memory allocation,
 * let the containers manage.
 * */

class BasicMatch;

struct Client {
    Client(uint32_t id, BasicMatch& sc, Socket* sock, bool is_snap_request)
        : sc_(sc), id_(id), is_snap_request_(is_snap_request), sock_(sock)
    {
        if( is_snap_request )
            sock_->set_recv_cb(this, snap_request_recv_cb);
        else
            sock_->set_recv_cb(this, recv_cb);

        sock_->set_error_cb(err_cb);
    }

    static void err_cb(void* user_data, int err) {
        std::cerr << "err on client connect <" << err << "> " << strerror(err) << std::endl;
    }

    static void recv_cb(void *user_data, uint8_t *buf, uint16_t& bytes);
    static void snap_request_recv_cb(void *user_data, uint8_t *buf, uint16_t& bytes);

    BasicMatch& sc_;
    uint32_t id_;
    bool is_snap_request_;
    std::unique_ptr<Socket> sock_;
};

class BasicMatch {
public:
    BasicMatch(const std::string& listen_addr, const std::string& snap_request_listen_addr, const std::string& md_send_addr)
        : engine_({this, &BasicMatch::on_fill}, {this, &BasicMatch::on_submit}, {this, &BasicMatch::on_cancel}),
          client_acceptor_(listen_addr, SocketAddr()), snap_request_acceptor_(snap_request_listen_addr, SocketAddr()), md_sock_(SocketAddr(), md_send_addr) {

        client_acceptor_.set_accept_cb(this, &BasicMatch::client_accept_cb);
        snap_request_acceptor_.set_accept_cb(this, &BasicMatch::snap_request_accept_cb);

        if(!client_acceptor_.listen())
            std::cerr << "failed to start listing to order connections" << std::endl;
        else if(!snap_request_acceptor_.listen())
            std::cerr << "failed to start listing for snapshot requests" << std::endl;
        else if(!md_sock_.init_send())
            std::cerr << "failed to open muliticast socket for sending market data" << std::endl;
        else
            std::cerr << "construction complete" << std::endl;
    }

    void on_fill(const std::string& symbol, uint64_t txn_id, uint16_t qty, double price) {
        send(md_sock_, Trade(txn_id, qty, price, symbol.c_str()));
    }

    void on_submit(const std::string& symbol, uint64_t txn_id, uint32_t id,  side_t side, uint16_t qty, double price) {
        send(md_sock_, Submit(txn_id, id, qty, price, (uint8_t)side, symbol.c_str()));
    }

    void on_cancel(const std::string& symbol, uint64_t txn_id, uint16_t qty, double price) {
        send(md_sock_, Cancel(txn_id, qty, price, symbol.c_str()));
    }

    void handle_msg(const Msg& msg) {
        results_t results = action(msg.get_text());
        for (results_t::const_iterator it=results.begin(); it!=results.end(); ++it) {
            std::cout << *it << std::endl;
        }
    }

    inline void add_conn(Socket* raw, bool is_snap_request) {
        uint32_t id = clId_++;

        clients_.emplace(std::piecewise_construct,
            std::forward_as_tuple(id),
            std::forward_as_tuple(id, *this, raw, is_snap_request)
        );
    }

    static void client_accept_cb(void *user_data, Socket *raw) {
        BasicMatch& bmatch = *(BasicMatch*)user_data;

        bmatch.add_conn(raw, false);

        std::cerr << "accepted client connection" << std::endl;
    }

    static void snap_request_accept_cb(void *user_data, Socket *raw) {
        BasicMatch& bmatch = *(BasicMatch*)user_data;

        bmatch.add_conn(raw, true);

        std::cerr << "accepted snap request connection" << std::endl;
    }

    void remove_client(uint32_t clId) {
        clients_.erase(clId);
    }

    //parse input
    //feed to engine
    //format engine output
    //
    results_t action(const std::string& line) {
        order_action_t a;
        if (!serializer_.deserialize(line, a))
            return (serializer_.serialize(a));

        results_.clear();

        if (engine_.execute(a, results_) > 0)
            return (serializer_.serialize(results_));
        return (results_t());
    }

    book_t& get_book(const char* symbol) { return engine_.get_book(symbol); }

    void handle_snap_request(Client& requestor, const char* symbol, uint16_t levels) {

        book_t& book = engine_.get_book(symbol);

        SnapResponse snap(book.txn_id_, symbol);

        //XXX create level object, with list and cumm qty to avoid the inner loops

        uint16_t i = 0;
        for (auto it = book.sells_.rbegin(); it != book.sells_.rend() && i < levels; ++it, ++i) {
            uint32_t cumm_qty = 0;

            for (auto& o : it->second)
                cumm_qty += o.qty_;

            snap.add_level(side_t::SELL, cumm_qty, it->first);
        }

        i = 0;
        for (auto it = book.buys_.rbegin(); it != book.buys_.rend() && i < levels; ++it, ++i) {
            uint32_t cumm_qty = 0;

            for (auto& o : it->second)
                cumm_qty += o.qty_;

            snap.add_level(side_t::BUY, cumm_qty, it->first);
        }

        send(*static_cast<TcpSocket*>(requestor.sock_.get()), snap);
    }

private:

    std::vector<order_action_t> results_;
    Engine engine_;
    Serializer serializer_;
    TcpSocket client_acceptor_;
    TcpSocket snap_request_acceptor_;
    UdpSocket md_sock_;
    uint32_t clId_ = 1;

    std::unordered_map<uint32_t, Client> clients_;
};

void Client::recv_cb(void *user_data, uint8_t *buf, uint16_t& bytes) {
    Client& cl = *(Client*)user_data;
    BasicMatch& bmatch = cl.sc_;

    const Msg* msg;

    while (bytes) {

        if ((msg = Msg::read_s(buf, bytes)))
            bmatch.handle_msg(*msg);
        else {
            break;
        }

        buf   += msg->get_size();
        bytes -= msg->get_size();
    }
}


void Client::snap_request_recv_cb(void *user_data, uint8_t *buf, uint16_t& bytes) {
    Client& cl = *(Client*)user_data;
    BasicMatch& bmatch = cl.sc_;

    const MdMsg* msg;
    const SnapRequest* req;

    while(bytes) {
        if((msg = req = SnapRequest::read_s(buf, bytes))) {
            bmatch.handle_snap_request(cl, req->symbol(), req->levels());
            //bmatch.remove_client(cl.id_);
        } else {
             if((msg = MdMsg::read_s(buf, bytes)) ) {
                std::cerr << "<== UNKNOWN type <" << msg->get_type() << ">" << std::endl;
             } else {
                std::cerr << "malformed data " << std::endl;
                break;
             }
        }

        buf   += msg->get_size();
        bytes -= msg->get_size();
    }
}


//main function takes command line argument as file name to try to read
int main(int argc, char **argv) {
                        //client accept addr  //md send addr
    BasicMatch bmatch("192.168.1.104:6677", "192.168.1.104:6678", "237.3.1.2:11111");

    Socket::start_recv_loop_s();
    return 0;
}
