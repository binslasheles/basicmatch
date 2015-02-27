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
    Client(uint32_t clId, BasicMatch& sc, Socket* sock) : sc_(sc), clId_(clId), sock_(sock) {
        sock_->set_recv_cb(&sc_, recv_cb);
        sock_->set_error_cb(err_cb);
    } 

    static void err_cb(void* user_data, int err) {
        std::cerr << "err on client connect" << std::endl;
    } 

    static void recv_cb(void *user_data, uint8_t *buf, uint16_t& bytes);

    BasicMatch& sc_;
    uint32_t clId_;
    std::unique_ptr<Socket> sock_;
};



class BasicMatch {
public:
    BasicMatch(const std::string& listen_addr, const std::string& md_send_addr) 
        : engine_({this, &BasicMatch::on_fill}, {this, &BasicMatch::on_submit}, {this, &BasicMatch::on_cancel}), 
          acceptor_(listen_addr, SocketAddr()), md_sock_(SocketAddr(), md_send_addr) { 

        acceptor_.set_accept_cb(this, &BasicMatch::accept_cb);

        if(!acceptor_.listen())
            std::cerr << "failed to start listing to order connections" << std::endl;
        else if(!md_sock_.init_send())
            std::cerr << "failed to open muliticast socket for sending market data" << std::endl;
        else 
            std::cerr << "construction complete" << std::endl;
    }

    void send(const MdMsg& m) {
        md_sock_.send(m.get_data(), m.get_size());
    }

    void on_fill(const std::string& symbol, uint64_t txn_id, uint16_t qty, double price) { 
        send(Trade(txn_id, qty, price, symbol.c_str())); 
    }

    void on_submit(const std::string& symbol, uint64_t txn_id, uint32_t id,  side_t side, uint16_t qty, double price) {
        send(Submit(txn_id, id, qty, price, (uint8_t)side, symbol.c_str())); 
    } 

    void on_cancel(const std::string& symbol, uint64_t txn_id, uint16_t qty, double price) {
        send(Cancel(txn_id, qty, price, symbol.c_str())); 
    } 

    void handle_msg(const Msg& msg) {
        results_t results = action(msg.get_text());
        for (results_t::const_iterator it=results.begin(); it!=results.end(); ++it) {
            std::cout << *it << std::endl;
        }
    }

    static void accept_cb(void *user_data, Socket *s) {
        BasicMatch& bmatch = *(BasicMatch*)user_data;

        uint32_t id = bmatch.clId_++;

        bmatch.clients_.emplace(std::piecewise_construct, 
            std::forward_as_tuple(id), 
            std::forward_as_tuple(id, bmatch, s)
        );

        std::cerr << "accepted client connection" << std::endl;
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

private:

    std::vector<order_action_t> results_;
    Engine engine_;
    Serializer serializer_;
    TcpSocket acceptor_;
    UdpSocket md_sock_;
    uint32_t clId_ = 1;
    std::unordered_map<uint32_t, Client> clients_;
};

void Client::recv_cb(void *user_data, uint8_t *buf, uint16_t& bytes) {
    BasicMatch& bmatch = *(BasicMatch*)user_data;

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


//main function takes command line argument as file name to try to read 
int main(int argc, char **argv) {
                        //client accept addr  //md send addr
    BasicMatch bmatch("192.168.1.104:6677", "237.3.1.2:11111");

    Socket::start_recv_loop_s();
    return 0;
}
