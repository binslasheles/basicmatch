#include "serializer.h"
#include <stdio.h>
#include <iterator>

void Serializer::set_stream(const std::string& line) 
{
    ss_.clear();
    ss_.str(line);
}

bool Serializer::deserialize(const std::string& line, order_action_t& o) 
{
    if (line.size())
    {
        set_stream(line);

        //ss >> stype;
        //action_type_t type = action_type_t(stype);
        //if (type != action_type_t::SUBMIT || type != action_type_t::ERR || type != action_type_t::CANCEL)
        //    return (false);
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
