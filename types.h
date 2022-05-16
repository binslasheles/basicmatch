#ifndef __SC_TYPES__H__
#define __SC_TYPES__H__
#include <list>
#include <string>
#include <list>

enum class ActionType : uint8_t
{
    NONE=0,
    ERR='E',
    CANCEL='X',
    SUBMIT='O',
    FILL='F',
    PRINT='P',
    TRADE=FILL
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

//symbol in place. On 32bit ptr systems,
//there might be more relevant savings
//from using flyweight(ptr to symbol)
struct OrderInfo {
    uint32_t id_;
    char symbol_[9];
    side_t side_;
    uint16_t qty_;
    double price_;

    inline bool crosses(double price) { return ((side_ == side_t::BUY) ? price <= price_ : price >= price_); }
};

typedef OrderInfo order_info_t;

struct ErrorInfo {
    uint32_t id_;
    const char *msg_;
};

typedef ErrorInfo error_info_t;

struct OrderAction
{
    OrderAction()=default;

    //provide constructors for common actions
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
    }; //always on or the other, reuse space
};

typedef OrderAction order_action_t;

typedef std::list<std::string> results_t;

#endif
