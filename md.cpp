// Stub implementation and example driver for Md.
// Your crossing logic should be accesible from the Md class.
// Other than the signature of Md::action() you are free to modify as needed.
#include <string>
#include <memory>
#include <iostream>
#include <list>
#include <unordered_map>
#include <msgpack.hpp>
#include "st_socket.h"
#include "types.h"
#include "msg.h"
#include "level_book.h"
#include "ls_autobahn.h"

/* */

class Md {
public:
    Md(const std::string& md_recv_addr, const std::string& snap_request_addr, const std::string& session_addr, const std::string& realm) 
        : s_(session_addr, realm, {this, &Md::on_join}, {this, &Md::on_goodbye}), 
          md_sock_(SocketAddr(), md_recv_addr), snap_sock_(SocketAddr(), snap_request_addr) {  }

    //or just pre allocate books...
    level_book_t& get_book(const std::string& symbol ) {

        if (books_.find(symbol) == books_.end())
            books_.emplace(std::piecewise_construct, 
                std::forward_as_tuple(symbol), 
                std::forward_as_tuple(symbol, level_book_t::level_update_cb_t(this, &Md::on_level_update), level_book_t::snapshot_cb_t(this, &Md::on_new_levels))
            );

        return (books_[symbol]);
    }

    inline void on_snapshot(const std::string& symbol, uint64_t txn_id, const SnapResponse::Level* levels) {

        level_book_t& book = get_book(symbol);

        book.clear();

        for(int i = 0; i < SnapResponse::MAX_LEVELS; ++i) {

            const SnapResponse::Level& l = levels[i];

            if(l.side_ == side_t::NONE)
                break;

            if( (side_t)l.side_ == side_t::BUY ) {
                book.add_buy(l.price_, l.qty_);
                std::cerr << "BUY " << l.qty_ << "@" << l.price_ << std::endl;
            } else {
                std::cerr << "SELL " << l.qty_ << "@" << l.price_ << std::endl;
                book.add_sell(l.price_, l.qty_);
            } 
        }

        /* for(action in recorded actions) {
         *    apply_action(book);
         * }
         */
        
        //std::cerr << "<== snapshot response" << std::endl; 

        recovering_ = false;
        txn_id_     = txn_id;
        on_new_levels(book);
    } 

    inline void on_trade(const std::string& symbol, uint64_t txn_id, uint16_t qty, double price) {  

        if(recovering_) {
            //record trade
            return;
        }

        level_book_t& book = get_book(symbol);

        if(txn_id - txn_id_ > 1) {
            recover(symbol);
        } else 
            txn_id_ = txn_id;
        
        if(price == book.best_buy())  {
            book.trade_buy(price, qty);
        } else {
            book.trade_sell(price, qty);
        }

        std::cerr << "<== trade symbol <" << symbol << "> txn <" << txn_id << "> qty <" << qty 
                << "> price <" << price << ">" << std::endl;
    } 

    inline void on_cancel(const std::string& symbol, uint64_t txn_id, uint16_t qty, double price) {

        if(recovering_) {
            //record cancel
            return;
        }

        level_book_t& book = get_book(symbol);

        if(txn_id - txn_id_ > 1) {
            recover(symbol);
        } else
            txn_id_ = txn_id;

        if(price < book.best_sell())  {
            book.cancel_buy(price, qty);
        } else {
            book.cancel_sell(price, qty);
        }

        std::cerr << "<== cancel symbol <" << symbol << "> txn <" << txn_id << "> qty <" << qty 
                << "> price <" << price << ">" <<  std::endl;
    }

    inline void on_submit(const std::string& symbol, uint64_t txn_id, uint8_t side, uint16_t qty, double price) {

        if(recovering_) {
            //record submit
            return;
        }

        level_book_t& book = get_book(symbol);

        if(txn_id - txn_id_ > 1 ) {
            recover(symbol);
            return;
        } else 
            txn_id_ = txn_id;

        if( (side_t)side == side_t::BUY ) {
            book.add_buy(price, qty);
        } else {
            book.add_sell(price, qty);
        } 

        std::cerr << "<== submit symbol <" << symbol << "> txn <" << txn_id << "> side <" << side 
            << "> qty <" << qty << "> price <" << price << ">" << std::endl;
    } 

    static void recv_cb(void *user_data, uint8_t *buf, uint16_t& bytes) {
        Md& md = *(Md*)user_data;

        const MdMsg* msg;
        const Submit* sbt;
        const Cancel* can;
        const Trade* tde;

        while(bytes) {
            if((msg = sbt = Submit::read_s(buf, bytes))) { 
                md.on_submit(sbt->symbol(), sbt->txn_id(), (uint8_t)sbt->side(), sbt->qty(),  sbt->price());
            } else if((msg = can = Cancel::read_s(buf, bytes)) ){ 
                md.on_cancel(can->symbol(), can->txn_id(), can->qty(), can->price());
            } else if((msg = tde = Trade::read_s(buf, bytes)) ){ 
                md.on_trade(tde->symbol(), tde->txn_id(), tde->qty(), tde->price());
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

    static void snap_recv_cb(void *user_data, uint8_t *buf, uint16_t& bytes) {
        Md& md = *(Md*)user_data;
        const MdMsg* msg;
        const SnapResponse* snp;
        //std::cerr << "===== received snapshot response ======" << std::endl;
        while(bytes) {
            if((msg = snp = SnapResponse::read_s(buf, bytes))) { 
                md.on_snapshot(std::string(snp->symbol()), snp->txn_id(), snp->levels());
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

    void on_new_levels(const level_book_t& book) {

        if(recovering_)
            return;

        std::vector<Autobahn::object> args;
        args.emplace_back(book.symbol_);
        vec_levels(book.symbol_, args);
        s_.publish("com.simple_cross.snapshot", args); 
        //send snapshot
    }

    void on_level_update(const level_book_t& book, side_t side, uint16_t qty, double price, action_type_t action) {

        if(recovering_)
            return;

        if(action == action_type_t::CANCEL) {
            s_.publish("com.simple_cross.cancel", {
                Autobahn::object(book.symbol_), 
                Autobahn::object(book.book_id_), 
                Autobahn::object(qty), 
                Autobahn::object(price)
            }); 
        } else if (action == action_type_t::SUBMIT) {
            s_.publish("com.simple_cross.submit", {
                Autobahn::object(book.symbol_), 
                Autobahn::object(book.book_id_), 
                Autobahn::object((char)side), 
                Autobahn::object(qty), 
                Autobahn::object(price)
            }); 

        } else /*trade*/ {
            s_.publish("com.simple_cross.trade", {
                Autobahn::object(book.symbol_), 
                Autobahn::object(book.book_id_), 
                Autobahn::object(qty), 
                Autobahn::object(price)
            });
        }
    }

    void vec_levels(const std::string& symbol, std::vector<Autobahn::object>& out) {
        auto it = books_.find(symbol);

        if(it != books_.end()) {

            level_book_t& book = it->second;

            std::vector<std::pair<uint32_t, double>> sellsv, buysv;

            for(int i = 0; i < book.levels(); ++i) {
                sellsv.emplace_back(book.sells_[i].qty_, book.sells_[i].qty_ ? book.sells_[i].price_ : 0);
                buysv.emplace_back(book.buys_[i].qty_, book.buys_[i].qty_ ? book.buys_[i].price_ : 0);
            }

            /*for(int i = 0; i < book.levels(); ++i) {
                std::cerr << sellsv[i].first  << "@" << sellsv[i].second  << std::endl;
                std::cerr << buysv[i].first  << "@" <<  buysv[i].second  << std::endl;
            }*/

            Autobahn::objbuf z;
            //Autobahn::object sells(sellsv, &z);
            //Autobahn::object buys(buysv, &z);
            out.emplace_back(book.book_id_ - 1);
            out.emplace_back(sellsv, &z);
            out.emplace_back(buysv, &z);

            //return {Autobahn::object(book.book_id_ - 1), sells, buys};
        }

        //return std::vector<Autobahn::object>();
    }

    std::vector<Autobahn::object> levels_proc(const std::vector<Autobahn::object>& args, const std::map<std::string, Autobahn::object>&) {
        //std::cerr << "============== VEC HANDLER ==============" << std::endl;
        //
        const std::string& symbol = args[0].as<std::string>(); 
        std::vector<Autobahn::object> vlevels;

        if(!recovering_)
            vec_levels(symbol, vlevels);

        return vlevels;
    }

    void on_join(uint64_t session_id) {
        md_sock_.set_recv_cb(this, &Md::recv_cb);
        snap_sock_.set_recv_cb(this, &Md::snap_recv_cb);

        if (!md_sock_.init_recv()) {
            std::cerr << "failed to start listening to market data" << std::endl;
            s_.leave();
            return;
        }

        std::cerr << "listening" << std::endl;
        s_.provide("com.simple_cross.levels",  Autobahn::session::endpoint_v_t(this, &Md::levels_proc));
    }

    void on_goodbye(const std::string& msg) { 
        //std::cerr << "received goodbye because <" << msg << ">" << std::endl; 
    }

    void recover(const std::string& symbol) { 
        recovering_ = true;

        if( !snap_sock_.connect() )
            std::cerr << "couldn't request snapshot" << std::endl;
        else {
            send(snap_sock_, SnapRequest(symbol.c_str(), 16));
        }
    } 

private:
    uint64_t txn_id_ = 0;
    bool recovering_ = false;
    Autobahn::session s_;
    UdpSocket md_sock_;
    TcpSocket snap_sock_;
    std::unordered_map<std::string, level_book_t> books_;
};

//main function takes command line argument as file name to try to read 
int main(int argc, char **argv) {
               //md addr          //snap req addr       //wamp session addr    //wamp realm
    Md scross("237.3.1.2:11111", "192.168.1.104:6678", "192.168.1.104:9091", "realm1");

    Socket::start_recv_loop_s();
    return 0;
}
