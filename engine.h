#ifndef __ENGINE_H__
#define __ENGINE_H__
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include "book.h"
#include "types.h"


/* The engine class contains all the actual matching logic.  It
 * keeps one book per instrument/symbol.  Engine expects in order
 * iteration of levels from book, as well as queue accessors from
 * book levels */
class Engine
{
public:
    uint32_t execute(order_action_t o, std::vector<order_action_t>&);

private:
    void fill(order_info_t& small, order_info_t& large, double price,
        std::vector<order_action_t>& results);

    inline book_t& get_book(const char *symbol); 

    inline void handle_cancel(uint32_t order_id, std::vector<order_action_t>& results);
    inline void handle_submit(order_info_t&, std::vector<order_action_t>& results);

    //buys, sells are different types so templating minimizes
    //code duplication
    template<typename Levels>
    void match(order_info_t&, Levels& levels, std::vector<order_action_t>& results);

    //no sorting requirement, want amortized o(1) lookup, insertion
    //so prefer hash-set to tree-set
    std::unordered_set<uint32_t> completed_orders_; 

    //no sorting requirement, want amortized o(1) lookup so prefer hash-map
    //keep iterator to order instead of ptr for o(1) removal from 
    //containing level (list)
    std::unordered_map<uint32_t, book_t::order_ref_t> orders_;

    //no sorting requirement, want amortized o(1) lookup so prefer hash-map to tree-map
    //data type is book
    std::unordered_map<std::string, book_t> books_;
};

#endif
