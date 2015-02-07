#ifndef __SC_TYPES__H__
#define __SC_TYPES__H__
#include <list>
#include <string>
#include <map>
#include <deque>
#include <limits>
#include <iostream>

enum class ActionType : uint8_t 
{
    ERR='E', 
    CANCEL='X', 
    SUBMIT='O', 
    FILL='F'
};

typedef ActionType action_type_t;

enum class Side : uint8_t 
{ 
    NONE=0,
    BUY='B', 
    SELL='S', 
    BID=BUY, 
    ASK=SELL 
};

typedef Side side_t;

struct OrderInfo 
{
    uint32_t id_;
    const char *symbol_;
    side_t side_;
    uint16_t qty_;
    double price_;

    inline
    bool price_match(double price)
    {
        //std::cerr << price << std::endl;
        return ((side_ == side_t::BUY) ? price <= price_ : price >= price_);  
    }
};

typedef OrderInfo order_info_t;

struct OrderAction
{
    OrderAction()=default;

    OrderAction(action_type_t type, const order_info_t& order_info) 
        : type_(type), order_info_(order_info) { }  

    explicit OrderAction(char *msg) 
        : type_(action_type_t::ERR), msg_(msg) { }  

    action_type_t type_;
    union 
    {
        order_info_t order_info_;
        const char *msg_;
    };
};

typedef OrderAction order_action_t;

struct Book 
{
    //typedef std::map<double, std::deque<order_info_t>> levels_t;
    ///typedef std::pair<levels_t::iterator, levels_t::iterator> sell_range_t;
    //typedef std::pair<levels_t::reverse_iterator, levels_t::reverse_iterator> buy_range_t;

    Book()=default;
    Book(const std::string& symbol) : symbol_(symbol) { }

    void add_order(const order_info_t& ord);
    void dump();

    //buy_range_t buy_range();
    //sell_range_t sell_range();

    std::string symbol_;
    std::map<double, std::deque<order_info_t>> sells_;
    std::map<double, std::deque<order_info_t>, std::greater<double>> buys_;
};

typedef Book book_t;
typedef std::list<std::string> results_t;

#endif
