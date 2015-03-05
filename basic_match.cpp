#include <string>
#include <fstream>
#include <iostream>
#include <list>
#include "types.h"
#include "serializer.h"
#include "engine.h"

/* BasicMatch is the driver for decoupled 
 * engine/matching logic and raw input deserialization. 
 * support for multiple input formats (possibly simultaneously)
 * only requires more/different serializers 
 *
 *
 * in all classes, minimize explicit memory allocation,
 * let the containers manage.
 * */


class BasicMatch
{
public:
    BasicMatch() { results_.reserve(16); }
    
    //parse input 
    //feed to engine  
    //format engine output
    results_t action(const std::string& line)
    { 
        order_action_t a;
        if (!serializer_.deserialize(line, a))
            return (serializer_.serialize(a));

        results_.clear();

        if (engine_.execute(a, results_) > 0)
            return (serializer_.serialize(results_)); 

        return (results_t());
    }

private:
    std::vector<order_action_t> results_;
    Engine engine_;
    Serializer serializer_;
};

//main function takes command line argument as file name to try to read 
int main(int argc, char **argv)
{
    BasicMatch scross;
    std::string line;

    const char *fname = "actions.txt";
    if (argc > 1)
        fname = argv[1];

    std::ifstream actions(fname, std::ios::in);

    while (std::getline(actions, line))
    {
        results_t results = scross.action(line);
        for (results_t::const_iterator it=results.begin(); it!=results.end(); ++it)
        {
            std::cout << *it << std::endl;
        }
    }
    return 0;
}
