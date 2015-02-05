#include "engine.h"

void Book::add_order(const order_info_t& ord) { 
    levels_[ord.price_].emplace(ord); 
    levels_[ord.price_].back().symbol_ = symbol_.c_str(); 
}

Book::buy_range_t Book::buy_range() { return buy_range_t(levels_t::reverse_iterator(levels_.lower_bound(buy_max_)), levels_.rend());}
Book::sell_range_t Book::sell_range() { return sell_range_t(levels_.lower_bound(sell_min_), levels_.end());  }

template<typename Range>
void Engine::match(order_info_t& o, Range range, std::vector<order_action_t>& results)
{
    for (; range.first!=range.second; ++range.first) 
    {
        if (!o.price_match(range.first->first)) //should check buy/sell then do correct comparison
            break;

        std::queue<order_info_t>& level = range.first->second; 
        while (!level.empty())
        {
            order_info_t& resting = level.front();

            if (o.qty_ > resting.qty_) //o partial, resting complete
            {
                fill(resting, o, results);
                level.pop();
            } 
            else if (o.qty_ < resting.qty_) //o complete, resting partial fills
            {
                fill(o, resting, results);
                break;
            }
            else //o complete, resting complete
            {
                fill(resting, o, results);
                level.pop();
                break;
            }
        }
    }
}



std::vector<order_action_t> Engine::execute(order_action_t action) 
{
    std::vector<order_action_t> results;

    order_info_t& o = action.order_info_; 

    if (books_.find(o.symbol_) == books_.end())
        books_.emplace(std::piecewise_construct, std::forward_as_tuple(o.symbol_), std::forward_as_tuple(o.symbol_));

    auto& book = books_[o.symbol_];
    if (o.side_ == side_t::BUY)
        match(o, book.sell_range(), results);
    else
        match(o, book.buy_range(), results);
    

    /*
    auto range = book.buy_range(); 

    for (; range.first!=range.second; ++range.first) 
    {
        if (!o.price_match(range.first->first)) //should check buy/sell then do correct comparison
            break;

        std::queue<order_info_t>& level = range.first->second; 
        while (!level.empty())
        {
            order_info_t& resting = level.front();

            if (o.qty_ > resting.qty_) //o partial, resting complete
            {
                fill(resting, o, results);
                level.pop();
            } 
            else if (o.qty_ < resting.qty_) //o complete, resting partial fills
            {
                fill(o, resting, results);
                break;
            }
            else //o complete, resting complete
            {
                fill(resting, o, results);
                level.pop();
                break;
            }
        }

    }*/

    if (o.qty_)
        book.add_order(o);
    //XXX HANDLE UPDATE TO MIN AND MAX

    return std::vector<order_action_t>();
}

void Engine::fill(order_info_t& small, order_info_t& large, std::vector<order_action_t>& results)
{
    uint16_t fill_qty = small.qty_;
    large.qty_ -= fill_qty;
    small.qty_ = 0;

    results.emplace_back(action_type_t::FILL, small);
    results.back().order_info_.qty_ = fill_qty;

    results.emplace_back(action_type_t::FILL, large);
    results.back().order_info_.qty_ = fill_qty;

    //"F 10003 IBM 5 100.00000"
    //F -  requires OID, SYMBOL, FILL_QTY, FILL_PX
}
