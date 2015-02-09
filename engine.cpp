#include "engine.h"
#include <assert.h>
#include <iostream>


template<typename Levels>
void Engine::match(order_info_t& o, Levels& levels, std::vector<order_action_t>& results)
{
    auto it=levels.begin(), end=levels.end();
    while (it!=end) 
    {
        double level_price = it->first;
        if (!o.qty_ || !o.crosses(level_price)) //should check buy/sell then do correct comparison
            break;

        Book::level_t& level = it->second; 

        while (o.qty_)
        {
            order_info_t& resting = level.front();
            if (o.qty_ < resting.qty_) //o complete, resting partial fills
            {
                fill(o, resting, level_price, results);
                return;
            }

            //o complete, resting complete
            //o partial, resting complete
            fill(resting, o, level_price, results);
            level.pop_front();

            if (level.empty())
            {
                levels.erase(it++);//need to erase this level
                break;
            }
        }
    }
}

void Engine::handle_cancel(uint32_t order_id, std::vector<order_action_t>& results)
{
    auto it = orders_.find(order_id);

    if (it == orders_.end())
        results.emplace_back("no order found for id", order_id);
    else
    {
        order_info_t& o = *(it->second);

        book_t& book = get_book(o.symbol_);

        if (o.side_ == side_t::BUY)
            book.remove_buy(it->second);
        else
            book.remove_sell(it->second);

        orders_.erase(it);
        completed_orders_.insert(order_id);
        results.emplace_back(order_id);
    }
}

void Engine::handle_submit(order_info_t& o, std::vector<order_action_t>& results)
{
    if (orders_.find(o.id_) != orders_.end())
        results.emplace_back("duplciate order id", o.id_);
    else if (!o.qty_)
        results.emplace_back("invalid [0] qty for order", o.id_);
    else
    {
        book_t& book = get_book(o.symbol_);

        if (o.side_ == side_t::BUY)
            match(o, book.sells_, results);
        else
            match(o, book.buys_, results);
        
        if (o.qty_)
            book.add_order(o, orders_);
    }
}

void Engine::fill(order_info_t& small, order_info_t& large, double fill_price, 
        std::vector<order_action_t>& results)
{
    uint16_t fill_qty = small.qty_;
    large.qty_ -= fill_qty;
    small.qty_ = 0;

    results.emplace_back(action_type_t::FILL, small);
    results.back().order_info_.qty_ = fill_qty;
    results.back().order_info_.price_ = fill_price;

    results.emplace_back(action_type_t::FILL, large);
    results.back().order_info_.qty_ = fill_qty;
    results.back().order_info_.price_ = fill_price;

    orders_.erase(small.id_);
    completed_orders_.insert(small.id_);
}

book_t& Engine::get_book(const char *symbol)
{
    if (books_.find(symbol) == books_.end())
        books_.emplace(std::piecewise_construct, std::forward_as_tuple(symbol), std::forward_as_tuple(symbol));

    return (books_[symbol]);
} 

uint32_t Engine::execute(order_action_t action, std::vector<order_action_t>& results) 
{ 
    if (action.type_ == action_type_t::SUBMIT) 
        handle_submit(action.order_info_, results);
    else if (action.type_ == action_type_t::CANCEL)
        handle_cancel(action.order_info_.id_, results);
    else if (action.type_ == action_type_t::PRINT)
    {
        for (auto& book_pair : books_)
        {
            book_pair.second.dump(results); 
        }
    }
    else 
        assert(0 && "invalid action type in Engine::match");

    return (results.size());
}
