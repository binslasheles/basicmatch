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
    void dump() { 
        books_["ZNZ4"].dump();
    }

private:
    static void fill(order_info_t& small, order_info_t& large, 
        std::vector<order_action_t>& results);

    template<typename Range>
    void match(order_info_t&, Range range, std::vector<order_action_t>& results);
    std::unordered_set<uint32_t> order_ids_;
    std::unordered_map<std::string, Book> books_;
};

#endif
