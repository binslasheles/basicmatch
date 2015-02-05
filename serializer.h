#ifndef __SERIALIZER_H__
#define __SERIALIZER_H__

#include "types.h"
#include <vector>
#include <sstream>

class Serializer
{
public:
    bool deserialize(const std::string& line, order_action_t& o); 
    results_t serialize(const order_action_t& o);
    results_t serialize(const std::vector<order_action_t>& actions);

private:
    void set_stream(const std::string& line);
    std::stringstream ss_;
};

#endif
