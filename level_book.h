#ifndef __SC_LEVEL_BOOK_H__
#define __SC_LEVEL_BOOK_H__
#include <iostream>
#include <unistd.h>
#include <limits>
#include <map>
#include "delegate.h"

struct Level {
    Level() : qty_(0), price_(0.) { }
    Level(uint32_t qty, double price) : qty_(qty), price_(price) { }

    uint32_t qty_;
    double price_;
};

template <uint32_t t_levels_>
struct LevelBook {
    typedef delegate<void(const LevelBook&)> level_change_cb_t;

    LevelBook() : txn_id_(0) {
        for(uint32_t i = 0; i < t_levels_; ++i)
            sells_[i] =  {0, std::numeric_limits<double>::infinity()};

        outer_buys_[-std::numeric_limits<double>::infinity()] = 0;
        outer_sells_[std::numeric_limits<double>::infinity()] = 0;
    }

    LevelBook(const std::string& symbol, const level_change_cb_t& level_change_cb) : txn_id_(0), symbol_(symbol), level_change_cb_(level_change_cb) {
        for(uint32_t i = 0; i < t_levels_; ++i)
            sells_[i] =  {0, std::numeric_limits<double>::infinity()};

        outer_buys_[-std::numeric_limits<double>::infinity()] = 0;
        outer_sells_[std::numeric_limits<double>::infinity()] = 0;
    }
 

    inline uint32_t levels() const { return t_levels_; }

    const static uint32_t last_idx_s = t_levels_ - 1;
    uint64_t txn_id_;
    std::string symbol_;
    level_change_cb_t level_change_cb_;
    Level sells_[t_levels_];
    Level buys_[t_levels_];

    std::map<double, uint32_t> outer_sells_;
    std::map<double, uint32_t, std::greater<double>> outer_buys_;

    double best_buy() const { return buys_[0].price_; }
    double best_sell() const { return sells_[0].price_; }

    void dump() {
        std::cerr << "===================" << std::endl;
        for(int i = t_levels_ - 1; i >= 0; --i) {
            std::cerr << sells_[i].qty_ << "@" <<  sells_[i].price_ <<  std::endl;
        } 
        for(int i = 0; i < t_levels_; ++i) {
            std::cerr << buys_[i].qty_ << "@" <<  buys_[i].price_ <<  std::endl;
        } 
    }

    void add_buy(double price, uint32_t qty) {
        //int lvl = 0;
        //while(buys_[lvl].price_ > price) ++lvl; 
        for(int lvl = 0; lvl < t_levels_; ++lvl) {

            if(buys_[lvl].price_ > price)
                continue;

            if(price == buys_[lvl].price_)
                buys_[lvl].qty_ += qty;
            else {
                for(int i = last_idx_s; i > lvl; --i) {
                    buys_[i] = buys_[i - 1];
                }

                buys_[lvl] = {qty, price};
                level_change_cb_(*this);
            }

            return;
        }

        outer_buys_[price] += qty;
    }

    void remove_buy(double price, uint32_t qty) {
        //int lvl = 0;
        //while(buys_[lvl].price_ != price) ++lvl; 
        //
        for(int lvl = 0; lvl < t_levels_; ++lvl) {
            if(buys_[lvl].price_ == price) {

                buys_[lvl].qty_ -= qty;

                if( !buys_[lvl].qty_ ) {
                    for(int i = lvl; i < last_idx_s; ++i) {
                        buys_[i] = buys_[i + 1];
                    }

                    auto it = outer_buys_.begin();

                    buys_[last_idx_s].price_ = it->first;
                    buys_[last_idx_s].qty_   = it->second;

                    if(it->second) {
                        level_change_cb_(*this);
                        outer_buys_.erase(it);
                    }
                }

                return;
            }
        }

        outer_buys_[price] -= qty;
    }

    void add_sell(double price, uint32_t qty) {
        //int lvl = 0;
        //while(sells_[lvl].price_ < price) ++lvl; 

        for(int lvl = 0; lvl < t_levels_; ++lvl) {

            if(sells_[lvl].price_ < price)
               continue;

            if(price == sells_[lvl].price_)
                sells_[lvl].qty_ += qty;
            else {
                for(int i = last_idx_s; i > lvl; --i) {
                    sells_[i] = sells_[i - 1];
                }

                sells_[lvl] = {qty, price};
                level_change_cb_(*this);
            }

            return;
        }

        outer_sells_[price] += qty;
    }

    void remove_sell(double price, uint32_t qty) {
        //int lvl = 0;
        //while(sells_[lvl].price_ != price) ++lvl; 

        for(int lvl = 0; lvl < t_levels_; ++lvl) {
            if(sells_[lvl].price_ == price) {
                sells_[lvl].qty_ -= qty;

                if( !sells_[lvl].qty_ ) {
                    for(int i = lvl; i < last_idx_s; ++i) {
                        sells_[i] = sells_[i + 1];
                    }

                    //sells_[last_idx_s].price_ = std::numeric_limits<double>::infinity(); 
                    //sells_[last_idx_s].qty_  = 0;
                    //send_snapshot
                    //
                    auto it = outer_sells_.begin();

                    sells_[last_idx_s].price_ = it->first;
                    sells_[last_idx_s].qty_   = it->second;

                    if(it->second) {
                        level_change_cb_(*this);
                        outer_buys_.erase(it);
                    }
                }

                return;
            }
        }

        outer_sells_[price] -= qty;
    }

};

typedef LevelBook<4> level_book_t;

/*    BUY   |  PRICE |  SELL  |
 *          |--------+--------|
 *          | 101.00 | 7      |
 *          | 100.00 | 25     |
 *          | 99.00  | 193    |
 *          | 98.00  | depth = 3
 * |     14 | 97.00  | 0 
 * |      6 | 96.00  | 1
 * |     13 | 95.00  | 2   
 
 *
 *
 *
 */
#endif
