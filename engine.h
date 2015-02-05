#ifndef __ENGINE_H__
#define __ENGINE_H__
#include "types.h"
#include <vector>
#include <unordered_map>
#include <map>


class Engine
{
public:
    std::vector<order_action_t> execute(order_action_t o);

private:
    static void fill(order_info_t& small, order_info_t& large, 
        std::vector<order_action_t>& results);

    void set_min_max();

    template<typename Range>
    void match(order_info_t&, Range range, std::vector<order_action_t>& results);

    std::unordered_map<std::string, Book> books_;
};

#endif
