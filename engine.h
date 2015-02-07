#ifndef __ENGINE_H__
#define __ENGINE_H__
#include "types.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>


class Engine
{
public:
    std::vector<order_action_t> execute(order_action_t o);

    void dump(std::vector<order_action_t>& info) 
    { 
        for(auto& book_pair : books_)
            book_pair.second.dump(info); 
    }

private:
    void fill(order_info_t& small, order_info_t& large, 
        std::vector<order_action_t>& results);

    //void post_process_ord(order_ref_t& ref);
    //void post_process_ord(uint32_t );
    

    template<typename Levels>
    void match(order_info_t&, Levels& levels, std::vector<order_action_t>& results);

    std::unordered_set<uint32_t> complete_orders_; 
    std::unordered_map<uint32_t, book_t::order_ref_t> orders_;
    std::unordered_map<std::string, book_t> books_;
};

#endif
