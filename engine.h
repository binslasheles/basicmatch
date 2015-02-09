#ifndef __ENGINE_H__
#define __ENGINE_H__
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include "book.h"
#include "types.h"



class Engine
{
public:
    uint32_t execute(order_action_t o, std::vector<order_action_t>&);

private:
    void fill(order_info_t& small, order_info_t& large, double price,
        std::vector<order_action_t>& results);

    inline book_t& get_book(const char *symbol); 

    inline void handle_cancel(uint32_t order_id, std::vector<order_action_t>& results);
    inline void handle_submit(order_info_t&, std::vector<order_action_t>& results);

    template<typename Levels>
    void match(order_info_t&, Levels& levels, std::vector<order_action_t>& results);

    std::unordered_set<uint32_t> completed_orders_; 
    std::unordered_map<uint32_t, book_t::order_ref_t> orders_;
    std::unordered_map<std::string, book_t> books_;
};

#endif
