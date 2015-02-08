#ifndef __SERIALIZER_H__
#define __SERIALIZER_H__

#include "types.h"
#include <vector>
#include <sstream>
#include <regex>

class Serializer
{
public:
    bool deserialize(const std::string& line, order_action_t& o); 
    results_t serialize(const std::vector<order_action_t>& actions);
    results_t serialize(const order_action_t& action);
private:
    static std::string convert(const order_action_t& action);
    std::regex submit_fmt_{"O (\\d+) ([0-9a-zA-Z]{0,8}) ([BS]) (\\d+) ([-+]?[0-9]{0,7}\\.?[0-9]{0,5})"};
    std::regex cancel_fmt_{"X (\\d+)"};
    std::regex print_fmt_{"P"};
    std::smatch m_;
};

#endif
