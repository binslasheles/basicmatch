#include "../types.h"
#include "../serializer.h"
#include <string>
#include <fstream>
#include <iostream>
#include <assert.h>


int main()
{
    Serializer s;

    std::string line;
    std::ifstream ff("invalid.txt", std::ios::in);

    order_action_t a;
    while (std::getline(ff, line))
    {
        assert(!s.deserialize(line, a) && a.type_ == action_type_t::ERR);  //std::cout << line << std::endl;
        std::cout << "invalid: [" << line << "]" <<std::endl;    
    }

    ff.close();
    ff.clear();
    ff.open("valid.txt");

    while (std::getline(ff, line))
    {
        assert(s.deserialize(line, a) && a.type_ != action_type_t::ERR);  //std::cout << line << std::endl;
        std::cout << "valid: [" << line << "]" <<std::endl;    
    }
}
