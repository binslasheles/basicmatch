#include "serializer.h"
#include <stdio.h>
#include <iterator>
#include <regex>



bool Serializer::deserialize(const std::string& line, order_action_t& o) 
{
    if (line.size())
    {
        action_type_t type = *(action_type_t*)line.c_str(); 
        switch(type)
        {
            case action_type_t::SUBMIT:
            {
                if( std::regex_match(line, m_, submit_fmt_) && m[5] != "." )
                {
                    std::cout << m_[1] << " " << m_[2] << " "  
                        << m_[3] << " " << m_[4] << " " << m_[5] << std::endl; 
                }

                //O - place order, requires OID, SYMBOL, SIDE, QTY, PX
                break;
            }
            case action_type_t::CANCEL:
            {
                std::smatch m;
                if (std::regex_match(line, m_, cancel_fmt_))
                {

                }

                break;
            }
            case action_type_t::PRINT:
            {
                if (line.size() == 1)
                    o.type_ = action_type_t::PRINT;

                break;
            }

            default: break;
        }

        return (true);
    }

    o.type_  = action_type_t::ERR;
    //o.  = ;
   // o.  = ;
    return (false);
}

results_t Serializer::serialize(const std::vector<order_action_t>& actions)
{
    for (auto& o : actions)
    {
        
    }

    return (results_t());
}

results_t Serializer::serialize(const order_action_t& o) 
{
    return (results_t());
}
