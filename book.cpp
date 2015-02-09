#include "book.h"
#include <iostream>

void Book::dump(std::vector<order_action_t>& info)
{
    for (auto it = sells_.rbegin(); it != sells_.rend(); ++it)
    {
        for (auto& o : it->second)
        {
            info.emplace_back(action_type_t::PRINT, o);
        }
    }

    for (auto& level_pair : buys_)
    {
        for (auto& o : level_pair.second)
        {
            info.emplace_back(action_type_t::PRINT, o);
        }
    }
}

