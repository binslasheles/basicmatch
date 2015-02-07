#include "serializer.h"
#include <stdio.h>
#include <iterator>
#include <regex>
#include <assert.h>



bool Serializer::deserialize(const std::string& line, order_action_t& o) 
{
    if (line.size())
    {
        try
        {
            if (std::regex_match(line, m_, submit_fmt_) && m_[5] != ".")
            {
                //O 10005 IBM B 10 99.0
                //O - place order, requires OID, SYMBOL, SIDE, QTY, PX
                std::cout << m_[1] << " " << m_[2] << " "  
                    << m_[3] << " " << m_[4] << " " << m_[5] << std::endl; 
                
                o.order_info_.qty_   = (uint32_t)std::stoi(m_[4]); 
                o.order_info_.id_    = (uint32_t)std::stoi(m_[1]);
                o.order_info_.side_  = (m_[3] == "B" ? side_t::BUY : side_t::SELL);
                o.order_info_.price_ = std::stod(m_[5]);
                //o.order_info_.symbol_ = 

                return (true);
            }

            if (std::regex_match(line, m_, cancel_fmt_))
            {
                o.type_  = action_type_t::CANCEL;
                o.order_info_.id_ = std::stoi(m_[1]);
                return (true);
            }

            if (std::regex_match(line, m_, print_fmt_))
            {
                o.type_  = action_type_t::PRINT;
                return (true);
            }

        } catch(...) { }
    }

    o.type_  = action_type_t::ERR;
    o.error_info_.msg_ = "invalid input";
    return (false);
}

results_t Serializer::serialize(const std::vector<order_action_t>& actions)
{
    results_t results;
    char buf[128];

    for (auto& a : actions)
    {
        if (a.type_ == action_type_t::CANCEL)
            snprintf(buf, sizeof(buf) - 1, "X %u", a.order_info_.id_);
        else if (a.type_ == action_type_t::ERR)
            snprintf(buf, sizeof(buf) - 1, "E %u %s", a.error_info_.id_, a.error_info_.msg_);
        else
        {
            const order_info_t& o = a.order_info_;

            if (a.type_ == action_type_t::FILL)
                snprintf(buf, sizeof(buf) - 1, "F %u %s %u %7.5lf", o.id_, o.symbol_, o.qty_, o.price_);
            else if (a.type_ == action_type_t::PRINT)
                snprintf(buf, sizeof(buf) - 1, "P %u %s %c %u %7.5lf", o.id_, o.symbol_, (char)o.side_, o.qty_, o.price_);
            else
                assert(0 && "invalid order action type");
        }

        results.emplace_back(buf);
    }

    return (results);
}

results_t Serializer::serialize(const order_action_t& o) 
{
    return (results_t());
}
