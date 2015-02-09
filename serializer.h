#ifndef __SERIALIZER_H__
#define __SERIALIZER_H__

#include "types.h"
#include <vector>
#include <sstream>
#include <regex>

/* Turns (raw) string inputs into actions using regular expressions.     
 * Turns actions into strings for output*/
class Serializer
{
public:
    bool deserialize(const std::string& line, order_action_t& o); 
    results_t serialize(const std::vector<order_action_t>& actions);
    results_t serialize(const order_action_t& action);
private:
    static inline std::string convert(const order_action_t& action);

    //no backtracking or lookahead so the expressions are potentially 
    //o(n).  chosen  instead of c style io and stringstream iteration
    //for more straigtforward error detection abilities.    
    std::regex submit_fmt_{"O (\\d+) ([0-9a-zA-Z]{0,8}) ([BS]) (\\d+) ([-+]?[0-9]{0,7}\\.?[0-9]{0,5})"};
    std::regex cancel_fmt_{"X (\\d+)"};
    std::regex print_fmt_{"P"};
    std::smatch m_;
};

#endif
