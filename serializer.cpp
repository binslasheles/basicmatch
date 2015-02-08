#include "serializer.h"
#include <stdio.h>
#include <iterator>
#include <regex>
#include <assert.h>



bool Serializer::deserialize(const std::string& line, order_action_t& a) 
{
    if (line.size())
    {
        try
        {
            if (std::regex_match(line, m_, submit_fmt_) && m_[5] != ".")
            {
                //std::cout << m_[1] << " " << m_[2] << " "  
                //    << m_[3] << " " << m_[4] << " " << m_[5] << std::endl; 
                
                a.type_  = action_type_t::SUBMIT;
                a.order_info_.qty_   = (uint32_t)std::stoi(m_[4]); 
                a.order_info_.id_    = (uint32_t)std::stoi(m_[1]);
                a.order_info_.side_  = (m_[3] == "B" ? side_t::BUY : side_t::SELL);
                a.order_info_.price_ = std::stod(m_[5]);
                a.order_info_.symbol_ =  "ZNZ4";//XXX

                return (true);
            }

            if (std::regex_match(line, m_, cancel_fmt_))
            {
                a.type_  = action_type_t::CANCEL;
                a.order_info_.id_ = std::stoi(m_[1]);
                return (true);
            }

            if (std::regex_match(line, m_, print_fmt_))
            {
                a.type_  = action_type_t::PRINT;
                return (true);
            }

        } catch(...) { }
    }

    a.type_ = action_type_t::ERR;
    a.error_info_.id_ = 0;
    a.error_info_.msg_ = "invalid input";
    return (false);
}

std::string Serializer::convert(const order_action_t& a)
{
    char buf[128];

    if (a.type_ == action_type_t::ERR)
        snprintf(buf, sizeof(buf) - 1, "E %u %s", a.error_info_.id_, a.error_info_.msg_);
    else
    {
        const order_info_t& o = a.order_info_;

        if (a.type_ == action_type_t::FILL)
            snprintf(buf, sizeof(buf) - 1, "F %u %s %u %7.5lf", o.id_, o.symbol_, o.qty_, o.price_);
        else if (a.type_ == action_type_t::CANCEL)
            snprintf(buf, sizeof(buf) - 1, "X %u", o.id_);
        else if (a.type_ == action_type_t::PRINT)
            snprintf(buf, sizeof(buf) - 1, "P %u %s %c %u %7.5lf", o.id_, o.symbol_, (char)o.side_, o.qty_, o.price_);
        else
            assert(0 && "invalid order action type");
    }
    return std::string(buf);
}

results_t Serializer::serialize(const std::vector<order_action_t>& actions)
{
    results_t results;
    char buf[128];

    for (auto& a : actions)
    {
        /*if (a.type_ == action_type_t::ERR)
            snprintf(buf, sizeof(buf) - 1, "E %u %s", a.error_info_.id_, a.error_info_.msg_);
        else
        {
            const order_info_t& o = a.order_info_;

            if (a.type_ == action_type_t::FILL)
                snprintf(buf, sizeof(buf) - 1, "F %u %s %u %7.5lf", o.id_, o.symbol_, o.qty_, o.price_);
            else if (a.type_ == action_type_t::CANCEL)
                snprintf(buf, sizeof(buf) - 1, "X %u", o.id_);
            else if (a.type_ == action_type_t::PRINT)
                snprintf(buf, sizeof(buf) - 1, "P %u %s %c %u %7.5lf", o.id_, o.symbol_, (char)o.side_, o.qty_, o.price_);
            else
                assert(0 && "invalid order action type");
        }*/

        results.emplace_back(convert(a));
    }

    return (results);
}

results_t Serializer::serialize(const order_action_t& action)
{
    return {convert(action)};
}
