#ifndef __SC_TYPES__H__
#define __SC_TYPES__H__

enum class ActionType : uint16_t 
{
    ERR, CANCEL, SUBMIT
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
    side_t side;
    float price;
    uint16_t qty;
};

typedef OrderInfo order_info_t;

struct OrderAction
{
    action_type_t type_;
    uint32_t order_id_;
    union 
    {
        order_info_t order_info_;
        char msg_[sizeof(order_info_t)];
    };
};

typedef OrderAction order_action_t;

#endif
