#ifndef __SC_BOOK_H__
#define __SC_BOOK_H__
#include "types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <list>

struct Book 
{
    //lists allow insertion and deletion of previous
    //and prior elements without invalidating existing 
    //iterators, o(1) push_back and pop_front provide
    //queue properties
    typedef std::list<order_info_t> level_t;

    //exported, allows clients to remove 
    //order from level in o(1), however removal of 
    //level itself (if empty) will be log(n) due to
    //containing map
    typedef level_t::iterator order_ref_t;

    Book()=default;
    Book(const std::string& symbol) : symbol_(symbol) { }

    inline void add_order(const order_info_t& ord, std::unordered_map<uint32_t, order_ref_t>& orders)
    { 
        level_t& level = (ord.side_ == side_t::BUY) ? buys_[ord.price_] : sells_[ord.price_];
        level.push_back(ord); 
        orders[ord.id_] = --level.end(); 
    }

    void dump(std::vector<order_action_t>& info);

    inline void remove_sell(order_ref_t& it) { remove_order(sells_, it); };
    inline void remove_buy(order_ref_t& it) { remove_order(buys_, it); }

    std::string symbol_;

    //frequent iteration in key magnitude order, prefer not to pay for sort at each 
    //iteration time so use tree-map (uses default compare std::less)
    std::map<double, level_t> sells_;

    //frequent iteration in reverse key magnitude order, 
    //prefer not to pay for sort at each iteration time so use tree-map 
    //with reversed comparison function
    std::map<double, level_t, std::greater<double>> buys_;
private:
    template <typename T>
    inline void remove_order(T& levels, order_ref_t& it)
    {
        typename T::iterator lit = levels.find(it->price_); 

        lit->second.erase(it);

        if (lit->second.empty()) //cancel o(1) when level not empty afterward
            levels.erase(lit);   //log(n) otherwise
    }
};


typedef Book book_t;

#endif
