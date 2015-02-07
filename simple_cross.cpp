/*
SimpleCross - a process that matches internal orders

Overview:
    * Accept/remove orders as they are entered and keep a book of
      resting orders
    * Determine if an accepted order would be satisfied by previously
      accepted orders (i.e. a buy would cross a resting sell)
    * Output (print) crossing events and remove completed (fully filled)
      orders from the book

Inputs:
    A string of space separated values representing an action.  The number of
    values is determined by the action to be performed and have the following
    format:

    ACTION [OID [SYMBOL SIDE QTY PX]]

    ACTION: single character value with the following definitions
    O - place order, requires OID, SYMBOL, SIDE, QTY, PX
    X - cancel order, requires OID
    P - print sorted book (see example below)

    OID: positive 32-bit integer value which must be unique for all orders

    SYMBOL: alpha-numeric string value. Maximum length of 8.

    SIDE: single character value with the following definitions
    B - buy
    S - sell

    QTY: positive 16-bit integer value

    PX: positive double precision value (7.5 format)

Outputs:
    A list of strings of space separated values that show the result of the
    action (if any).  The number of values is determined by the result type and
    have the following format:

    RESULT OID [SYMBOL SIDE (FILL_QTY | OPEN_QTY) (FILL_PX | ORD_PX)]

    RESULT: single character value with the following definitions
    F - fill (or partial fill), requires OID, SYMBOL, FILL_QTY, FILL_PX
    X - cancel confirmation, requires OID
    P - book entry, requires OID, SYMBOL, SIDE, OPEN_QTY, ORD_PX (see example below)
    E - error, requires OID. Remainder of line represents string value description of the error

    FILL_QTY: positive 16-bit integer value representing qty of the order filled by
              this crossing event

    OPEN_QTY: positive 16-bit integer value representing qty of the order not yet filled

    FILL_PX:  positive double precision value representing price of the fill of this
              order by this crossing event (7.5 format)

    ORD_PX:   positive double precision value representing original price of the order (7.5 format)

Conditions/Assumptions:
    * The implementation should be a standalone Linux console application (include
      source files, testing tools and Makefile in submission)
    * The app should respond to malformed input and other errors with a RESULT
      of type 'E' and a descriptive error message
    * Development should be production level quality and performance. Design and
      implementation choices should documented
    * All orders are standard limit orders
    * Orders should be selected for crossing using price-time (FIFO) priority
    * Orders for different symbols should not cross (i.e. the book must support multiple symbols)

Example session:
    INPUT                                   | OUTPUT
    ============================================================================
    "O 10000 IBM B 10 100.00000"            | results.size() == 0
    "O 10001 IBM B 10 99.00000"             | results.size() == 0
    "O 10002 IBM S 5 101.00000"             | results.size() == 0
    "O 10003 IBM S 5 100.00000"             | results.size() == 2
                                            | results[0] == "F 10003 IBM 5 100.00000"
                                            | results[1] == "F 10000 IBM 5 100.00000"
    "O 10004 IBM S 5 100.00000"             | results.size() == 2
                                            | results[0] == "F 10004 IBM 5 100.00000"
                                            | results[1] == "F 10000 IBM 5 100.00000"
    "X 10002"                               | results.size() == 1
                                            | results[0] == "X 10002"
    "O 10005 IBM B 10 99.00000"             | results.size() == 0
    "O 10006 IBM B 10 100.00000"            | results.size() == 0
    "O 10007 IBM S 10 101.00000"            | results.size() == 0
    "O 10008 IBM S 10 102.00000"            | results.size() == 0
    "O 10008 IBM S 10 102.00000"            | results.size() == 1
                                            | results[0] == "E 10008 Duplicate order id"
    "O 10009 IBM S 10 102.00000"            | results.size() == 0
    "P"                                     | results.size() == 6
                                            | results[0] == "P 10009 IBM S 10 102.00000"
                                            | results[1] == "P 10008 IBM S 10 102.00000"
                                            | results[2] == "P 10007 IBM S 10 101.00000"
                                            | results[3] == "P 10006 IBM B 10 100.00000"
                                            | results[4] == "P 10001 IBM B 10 99.00000"
                                            | results[5] == "P 10005 IBM B 10 99.00000"
*/

// Stub implementation and example driver for SimpleCross.
// Your crossing logic should be accesible from the SimpleCross class.
// Other than the signature of SimpleCross::action() you are free to modify as needed.
#include <string>
#include <fstream>
#include <iostream>
#include <list>
#include "types.h"
#include "serializer.h"
#include "engine.h"


class SimpleCross
{
public:
    results_t action(const std::string& line)
    { 
        std::vector<order_action_t> v{
            { action_type_t::SUBMIT, {18, "ZNZ4", side_t::SELL, 10, 107.000 }},
            { action_type_t::SUBMIT, {17, "ZNZ4", side_t::SELL, 10, 106.000 }},
            { action_type_t::SUBMIT, {16, "ZNZ4", side_t::SELL, 10, 105.000 }},
            { action_type_t::SUBMIT, {15, "ZNZ4", side_t::SELL, 10, 104.000 }},
            { action_type_t::SUBMIT, {11, "ZNZ4", side_t::SELL, 10, 103.000 }},
            { action_type_t::SUBMIT, {14, "ZNZ4", side_t::SELL, 10, 102.000 }},
            { action_type_t::SUBMIT, {13, "ZNZ4", side_t::SELL, 10, 101.000 }},
            { action_type_t::SUBMIT, {12, "ZNZ4", side_t::SELL, 10, 100.000 }},
            { action_type_t::SUBMIT, {8, "ZNZ4", side_t::BUY, 18, 107.000 }},
            { action_type_t::SUBMIT, {7, "ZNZ4", side_t::BUY, 17, 106.000 }},
            { action_type_t::SUBMIT, {6, "ZNZ4", side_t::BUY, 16, 105.000 }},
            { action_type_t::SUBMIT, {5, "ZNZ4", side_t::BUY, 15, 104.000 }},
            { action_type_t::SUBMIT, {4, "ZNZ4", side_t::BUY, 14, 103.000 }},
            { action_type_t::SUBMIT, {3, "ZNZ4", side_t::BUY, 13, 102.000 }},
            { action_type_t::SUBMIT, {2, "ZNZ4", side_t::BUY, 12, 101.000 }},
            { action_type_t::CANCEL, {999, "", side_t::BUY, 12, 101.000 }},
        };

        for(auto& o : v)
        {
            for (auto& s : serializer_.serialize(engine_.execute(o)) )
            {
                std::cout << s << std::endl;
            } 
        }

        //std::vector<order_action_t> res;
        //engine_.dump(res);
        //for(auto & a : res)
        //{
        //    order_info_t& o = a.order_info_;
//
//            std::cerr << o.price_ << " " << "\t" << (char)o.side_ << " " << o.id_ << " " 
//                    << o.qty_ << "@" << o.price_ << std::endl;;
//        }

        return (results_t());
        /*order_action_t o;
        if (!serializer_.deserialize(line, o))
            return (serializer_.serialize(o));

        return (serializer_.serialize(engine_.execute(o))); 
        */
    }
private:
    Engine engine_;
    Serializer serializer_;
};

int main(int argc, char **argv)
{
    SimpleCross scross;
    std::string line;
    std::ifstream actions("actions.txt", std::ios::in);
    results_t results = scross.action(line);
    while (std::getline(actions, line))
    {
        //results_t results = scross.action(line);
        //for (results_t::const_iterator it=results.begin(); it!=results.end(); ++it)
        //{
        //    std::cout << *it << std::endl;
        //}
    }
    return 0;
}
