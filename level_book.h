#ifndef __SC_LEVEL_BOOK_H__
#define __SC_LEVEL_BOOK_H__
#include <iostream>
#include <unistd.h>
#include <limits>
#include <map>
#include "delegate.h"
#include "types.h"

struct Level {
    Level() : qty_(0), price_(0.) { }
    Level(uint32_t qty, double price) : qty_(qty), price_(price) { }

    uint32_t qty_;
    double price_;
};

template <uint32_t t_levels_>
struct LevelBook {
    typedef delegate<void(const LevelBook&)> snapshot_cb_t;
    typedef delegate<void(const LevelBook&, side_t, uint16_t, double, action_type_t)> level_update_cb_t;

    LevelBook()=default;

    LevelBook(const std::string& symbol, const level_update_cb_t& level_update_cb, const snapshot_cb_t& snapshot_cb) 
        : book_id_(0), symbol_(symbol),level_update_cb_(level_update_cb), snapshot_cb_(snapshot_cb) {
        for(uint32_t i = 0; i < t_levels_; ++i)
            sells_[i] =  {0, std::numeric_limits<double>::infinity()};

        outer_buys_[-std::numeric_limits<double>::infinity()] = 0;
        outer_sells_[std::numeric_limits<double>::infinity()] = 0;
    }
 

    inline uint32_t levels() const { return t_levels_; }

    void clear() {
        for(uint32_t i = 0; i < t_levels_; ++i) {
            sells_[i] = {0, std::numeric_limits<double>::infinity()};
            buys_[i] =  {0, -std::numeric_limits<double>::infinity()};
        }

        outer_buys_.erase(++outer_buys_.begin(), outer_buys_.end());
        outer_sells_.erase(++outer_sells_.begin(), outer_sells_.end());

        book_id_++;
    }

    const static uint32_t last_idx_s = t_levels_ - 1;
    uint64_t book_id_;
    std::string symbol_;
    level_update_cb_t level_update_cb_;
    snapshot_cb_t snapshot_cb_;
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

            if(price == buys_[lvl].price_) {
                level_update_cb_(*this, side_t::BUY, qty, price, action_type_t::SUBMIT);
                buys_[lvl].qty_ += qty;
            } else {
                for(int i = last_idx_s; i > lvl; --i) {
                    buys_[i] = buys_[i - 1];
                }

                buys_[lvl] = {qty, price};
                snapshot_cb_(*this);
            }
            ++book_id_;
            return;
        }

        outer_buys_[price] += qty;
    }

    inline void remove_buy(double price, uint32_t qty, action_type_t t) {
        //int lvl = 0;
        //while(buys_[lvl].price_ != price) ++lvl; 
        //
        for(int lvl = 0; lvl < t_levels_; ++lvl) {
            if(buys_[lvl].price_ == price) {

                buys_[lvl].qty_ -= qty;

                if( buys_[lvl].qty_ ) {
                    level_update_cb_(*this, side_t::BUY, qty, price, t);
                } else {
                    for(int i = lvl; i < last_idx_s; ++i) {
                        buys_[i] = buys_[i + 1];
                    }

                    auto it = outer_buys_.begin();

                    buys_[last_idx_s].price_ = it->first;
                    buys_[last_idx_s].qty_   = it->second;

                    if(it->second) {
                        snapshot_cb_(*this);
                        outer_buys_.erase(it);
                    }
                }

                ++book_id_;
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

            if(price == sells_[lvl].price_) {
                level_update_cb_(*this, side_t::SELL, qty, price, action_type_t::SUBMIT);
                sells_[lvl].qty_ += qty;
            } else {
                for(int i = last_idx_s; i > lvl; --i) {
                    sells_[i] = sells_[i - 1];
                }

                sells_[lvl] = {qty, price};
                snapshot_cb_(*this);
            }

            ++book_id_;
            return;
        }

        outer_sells_[price] += qty;
    }

    inline void remove_sell(double price, uint32_t qty, action_type_t t) {
        //int lvl = 0;
        //while(sells_[lvl].price_ != price) ++lvl; 

        for(int lvl = 0; lvl < t_levels_; ++lvl) {
            if(sells_[lvl].price_ == price) {
                sells_[lvl].qty_ -= qty;

                if( sells_[lvl].qty_ ) {
                    level_update_cb_(*this, side_t::SELL, qty, price, t);
                } else  {
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
                        snapshot_cb_(*this);
                        outer_buys_.erase(it);
                    }
                }

                ++book_id_;
                return;
            }
        }

        outer_sells_[price] -= qty;
    }

    inline void submit(side_t side, double price, uint32_t qty) {
        if( side == side_t::BUY ) {
            add_buy(price, qty);
        } else {
            add_sell(price, qty);
        } 
    }

    inline void trade(double price, uint32_t qty) {
        if(price == best_buy())  {
            remove_buy(price, qty, action_type_t::TRADE);
        } else {
            remove_sell(price, qty, action_type_t::TRADE);
        }
    }

    inline void cancel(double price, uint32_t qty) {
        if(price < best_sell())  {
            remove_buy(price, qty, action_type_t::CANCEL);
        } else {
            remove_sell(price, qty, action_type_t::CANCEL);
        }
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
