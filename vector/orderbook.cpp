#include <vector>
#include "limit.h"
#include "../lookup_table.h"
#include "order_pool.h"
#include "../message.h"

template<bool Side>
struct BookSide {};

template<>
struct BookSide<true> {
    using MapType = std::vector<std::pair<int32_t, Vector_Limit*>>;
    static constexpr auto compare = [](const std::pair<int32_t, Vector_Limit*>& a, int32_t b) {
        return a.first < b;
    };
};

template<>
struct BookSide<false> {
    using MapType = std::vector<std::pair<int32_t, Vector_Limit*>>;
    static constexpr auto compare = [](const std::pair<int32_t, Vector_Limit*>& a, int32_t b) {
        return a.first > b;
    };
};

class Vector_Orderbook {
private:
    BookSide<true>::MapType bids_;
    BookSide<false>::MapType offers_;
    OpenAddressTable<Order> order_lookup_;
    OrderPool order_pool_;

    static constexpr size_t INITIAL_LEVELS = 1000;
    static constexpr size_t INITIAL_ORDERS = 1000000;

    template<bool Side>
    typename BookSide<Side>::MapType& get_book_side() {
        if constexpr(Side) {
            return bids_;
        }
        else return offers_;
    }

public:
    Vector_Orderbook() : order_pool_(INITIAL_ORDERS) {
        bids_.reserve(INITIAL_LEVELS);
        offers_.reserve(INITIAL_LEVELS);
        order_lookup_.reserve(INITIAL_ORDERS);
    }

    template<bool Side>
    __attribute__((always_inline))
    Vector_Limit* find_or_insert_limit(int32_t price) {
        auto& levels = get_book_side<Side>();
        auto it = std::lower_bound(levels.begin(), levels.end(), price, BookSide<Side>::compare);

        if (it != levels.end() && it->first == price) {
            return it->second;
        }

        auto* limit = new Vector_Limit();
        levels.emplace(it, price, limit);
        return limit;
    }

    template<bool Side>
    __attribute__((always_inline))
    void add_order(uint64_t order_id, int32_t order_price, int32_t order_size, uint64_t order_time) {
        Order* new_order = order_pool_.get_order();
        new_order->id_ = order_id;
        new_order->price_ = order_price;
        new_order->size_ = order_size;
        new_order->unix_time_ = order_time;
        new_order->side_ = Side;

        auto curr_limit = find_or_insert_limit<Side>(order_price);
        curr_limit->add_order(new_order);
        new_order->parent_ = curr_limit;
        order_lookup_.insert(order_id, new_order);
    }

    template<bool Side>
    __attribute__((always_inline))
    void remove_order(uint64_t order_id, int32_t order_price, int32_t order_size) {
        auto target = *order_lookup_.find(order_id);
        auto parent_limit = target->parent_;
        parent_limit->remove_order(target);

        if (parent_limit->num_orders_ == 0) {
            auto& levels = get_book_side<Side>();
            auto it = std::lower_bound(levels.begin(), levels.end(), order_price,
                                       BookSide<Side>::compare);

            if (it->first == order_price) {
                levels.erase(it);
            }
        }

        order_lookup_.erase(order_id);
        order_pool_.return_order(target);
    }

    template<bool Side>
    __attribute__((always_inline))
    void modify_order(uint64_t order_id, int32_t new_price, int32_t new_size, uint64_t order_time) {
        auto* target = *order_lookup_.find(order_id);
        if (!target) {
            add_order<Side>(order_id, new_price, new_size, order_time);
            return;
        }

        if (target->side_ != Side) {
            throw std::runtime_error("Order changed sides");
        }

        auto old_price = target->price_;
        auto old_size = target->size_;

        if (old_price != new_price) {
            remove_order<Side>(order_id, old_price, old_size);
            add_order<Side>(order_id, new_price, new_size, order_time);
            return;
        }

        if (new_size > old_size) {
            remove_order<Side>(order_id, old_price, old_size);
            add_order<Side>(order_id, new_price, new_size, order_time);
            return;
        }

        target->size_ = new_size;
        target->unix_time_ = order_time;
    }


    __attribute__((always_inline))
    inline void process_msg(const message& msg) {
        switch (msg.action_) {
            case 'A':
                if (msg.side_) {
                    add_order<true>(msg.id_, msg.price_, msg.size_, msg.time_);
                } else {
                    add_order<false>(msg.id_, msg.price_, msg.size_, msg.time_);
                }
                break;

            case 'M':
                if (msg.side_) {
                    modify_order<true>(msg.id_, msg.price_, msg.size_, msg.time_);
                } else {
                    modify_order<false>(msg.id_, msg.price_, msg.size_, msg.time_);
                }
                break;

            case 'C':
                if (msg.side_) {
                    remove_order<true>(msg.id_, msg.price_, msg.size_);
                } else {
                    remove_order<false>(msg.id_, msg.price_, msg.size_);
                }
                break;
        }
    }

    int32_t get_best_bid_price() const { return bids_.rbegin()->first; }

    int32_t get_best_ask_price() const { return offers_.rbegin()->first; }

    uint32_t get_best_bid_volume() const { return bids_.rbegin()->second->volume_; }

    uint32_t get_best_ask_volume() const { return offers_.rbegin()->second->volume_; }
};