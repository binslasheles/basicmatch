#ifndef __ENGINE_H__
#define __ENGINE_H__
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include "book.h"
#include "delegate.h"
#include "types.h"


/* The engine class contains all the actual matching logic.  It
 * keeps one book per instrument/symbol.  Engine expects in order
 * iteration of levels from book, as well as queue accessors from
 * book levels */
class Engine
{
    typedef delegate<void(const std::string& symbol, uint64_t txn_id, uint16_t qty, double price)> fill_cb_t;
    typedef delegate<void(const std::string& symbol, uint64_t txn_id, uint16_t qty, double price)> cancel_cb_t;
    typedef delegate<void(const std::string& symbol, uint64_t txn_id, uint32_t id, side_t side, uint16_t qty, double price)> submit_cb_t;
public:

    Engine()=default;
    Engine(const fill_cb_t& fill_cb, const submit_cb_t& submit_cb, const cancel_cb_t& cancel_cb) : fill_cb_(fill_cb), submit_cb_(submit_cb), cancel_cb_(cancel_cb) { }

    uint32_t execute(order_action_t o, std::vector<order_action_t>&);
    inline book_t& get_book(const char *symbol) {
        if (books_.find(symbol) == books_.end())
            books_.emplace(std::piecewise_construct, std::forward_as_tuple(symbol), std::forward_as_tuple(symbol));

        return (books_[symbol]);
    }

private:
    void fill(order_info_t& small, order_info_t& large, double price, uint64_t txn_id,
        std::vector<order_action_t>& results);

    inline void handle_cancel(uint32_t order_id, std::vector<order_action_t>& results);
    inline void handle_submit(order_info_t&, std::vector<order_action_t>& results);
    fill_cb_t fill_cb_;
    submit_cb_t submit_cb_;
    cancel_cb_t cancel_cb_;

    //buys, sells are different types so templating minimizes
    //code duplication
    template<typename Levels>
    void match(order_info_t&, Levels& levels, uint64_t& txn_id, std::vector<order_action_t>& results);

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
