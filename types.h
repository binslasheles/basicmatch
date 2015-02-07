#ifndef __SC_TYPES__H__
#define __SC_TYPES__H__
#include <list>
#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <deque>
#include <limits>
#include <iostream>

enum class ActionType : uint8_t 
{
    ERR='E', 
    CANCEL='X', 
    SUBMIT='O', 
    FILL='F',
    PRINT='P'
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
    bool crosses(double price)
    {
        //std::cerr << price << std::endl;
        return ((side_ == side_t::BUY) ? price <= price_ : price >= price_);  
    }
};

typedef OrderInfo order_info_t;

struct ErrorInfo
{
    uint32_t id_;
    const char* msg_;
};

typedef ErrorInfo error_info_t;

struct OrderAction
{
    OrderAction()=default;

    OrderAction(action_type_t type, const order_info_t& order_info) 
        : type_(type), order_info_(order_info) { }  

    explicit OrderAction(const char *msg, uint32_t id=0) 
        : type_(action_type_t::ERR), error_info_{id, msg} { }  

    explicit OrderAction(uint32_t id) 
        : type_(action_type_t::CANCEL) { order_info_.id_ = id; }  

    action_type_t type_;
    union 
    {
        order_info_t order_info_;
        error_info_t error_info_;
    };
};

typedef OrderAction order_action_t;

struct Book 
{
    typedef std::deque<order_info_t>::iterator order_ref_t;

    Book()=default;
    Book(const std::string& symbol) : symbol_(symbol) { }

    void add_order(const order_info_t& ord, std::unordered_map<uint32_t, order_ref_t>& );
    void dump(std::vector<order_action_t>& info);
    
    std::string symbol_;
    std::map<double, std::deque<order_info_t>> sells_;
    std::map<double, std::deque<order_info_t>, std::greater<double>> buys_;
};

typedef Book book_t;
typedef std::list<std::string> results_t;

#endif
