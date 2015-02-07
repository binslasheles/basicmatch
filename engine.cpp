#include "engine.h"
#include <iostream>


void Book::dump()
{
    std::cerr << symbol_ << std::endl;
    for (auto& lp : sells_)
    {
        std::cerr << lp.first << " ";

        decltype(lp.second) q;
        while (!lp.second.empty() )
        {
            auto& o = lp.second.front();
            std::cerr << "\t" << (char)o.side_ << " " << o.id_ << " " 
                    << o.qty_ << "@" << o.price_ << std::endl;;

            q.push_back(o);
            lp.second.pop_front();
        }

        while (!q.empty() )
        {
            lp.second.push_back(q.front());
            q.pop_front();
        }
    } 

    for (auto& lp : buys_)
    {
        std::cerr << lp.first << " ";

        decltype(lp.second) q;
        while (!lp.second.empty() )
        {
            auto& o = lp.second.front();
            std::cerr << "\t" << (char)o.side_ << " " << o.id_ << " " 
                    << o.qty_ << "@" << o.price_ << std::endl;;

            q.push_back(o);
            lp.second.pop_front();
        }

        while (!q.empty() )
        {
            lp.second.push_back(q.front());
            q.pop_front();
        }
    }
}

void Book::add_order(const order_info_t& ord)
{ 
    std::deque<order_info_t>& level = (ord.side_ == side_t::BUY) ? buys_[ord.price_] : sells_[ord.price_];
    level.push_back(ord); 
    level.back().symbol_ = symbol_.c_str(); 
}

//Book::buy_range_t Book::buy_range() { return (buy_range_t(levels_t::reverse_iterator(levels_.lower_bound(buy_max_)), levels_.rend())); }
//Book::sell_range_t Book::sell_range() { return (sell_range_t(levels_.lower_bound(sell_min_), levels_.end()));  }

template<typename Levels>
void Engine::match(order_info_t& o, Levels& levels, std::vector<order_action_t>& results)
{
    //std::cerr << 
    auto it=levels.lower_bound(o.price_), end=levels.end();
    //for (; it!=end; ++it) 
    while (it!=end) 
    {
        if (!o.price_match(it->first)) //should check buy/sell then do correct comparison
            break;

        std::deque<order_info_t>& level = it->second; 
        //while (!level.empty())
        while (true)
        {
            if (level.empty())
            {
                std::cerr << "here" << std::endl;
                levels.erase(it++);//need to erase this level
                break;
            }

            ++it;
            order_info_t& resting = level.front();

            if (o.qty_ > resting.qty_) //o partial, resting complete
            {
                fill(resting, o, results);
                level.pop_front();
            } 
            else if (o.qty_ < resting.qty_) //o complete, resting partial fills
            {
                fill(o, resting, results);
                break;
            }
            else //o complete, resting complete
            {
                std::cerr << "complete" << std::endl;
                fill(resting, o, results);
                level.pop_front();
                break;
            }
        }
    }
}



std::vector<order_action_t> Engine::execute(order_action_t action) 
{
    order_info_t& o = action.order_info_; 

    if (books_.find(o.symbol_) == books_.end())
        books_.emplace(std::piecewise_construct, std::forward_as_tuple(o.symbol_), std::forward_as_tuple(o.symbol_));

    auto& book = books_[o.symbol_];

    std::vector<order_action_t> results;
    if (o.side_ == side_t::BUY)
        match(o, book.sells_, results);
    else
        match(o, book.buys_, results);
    
    if (o.qty_)
        book.add_order(o);

    return (results);
}

void Engine::fill(order_info_t& small, order_info_t& large, std::vector<order_action_t>& results)
{
    std::cerr << "fill " << small.qty_ << "@" << small.price_ << " -- "
        << large.qty_ << "@" << large.price_ << std::endl;

    uint16_t fill_qty = small.qty_;
    large.qty_ -= fill_qty;
//    small.qty_ = 0;

    results.emplace_back(action_type_t::FILL, small);
    results.back().order_info_.qty_ = fill_qty;

    results.emplace_back(action_type_t::FILL, large);
    results.back().order_info_.qty_ = fill_qty;

    //"F 10003 IBM 5 100.00000"
    //F -  requires OID, SYMBOL, FILL_QTY, FILL_PX
}
