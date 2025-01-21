#pragma once

#include <cstdint>
#include <map>
#include <unordered_map>
#include <boost/functional/hash.hpp>
#include <arm_neon.h>
#include <chrono>
#include "../lookup_table.h"
#include "map_order.cpp"
#include "map_limit.cpp"
#include "map_order_pool.cpp"
#include "../message.h"


template<bool Side>
struct MapBookSide {};

template<>
struct MapBookSide<true> {
    using MapType = std::map<int32_t, MapLimit*, std::greater<>>;
};

template<>
struct MapBookSide<false> {
    using MapType = std::map<int32_t, MapLimit*, std::less<>>;
};

class Orderbook {
private:
    MapOrderPool order_pool_;
    std::unordered_map<std::pair<int32_t, bool>, MapLimit*, boost::hash<std::pair<int32_t, bool>>> limit_lookup_;
    uint64_t bid_count_;
    uint64_t ask_count_;

    static constexpr size_t BUFFER_SIZE = 40000;
    size_t write_index_ = 0;

    template<bool Side>
    __attribute__((always_inline))
    typename MapBookSide<Side>::MapType& get_book_side() {
        if constexpr (Side) {
            return bids_;
        } else {
            return offers_;
        }
    }

    template<bool Side>
    __attribute__((always_inline))
    MapLimit* get_or_insert_limit(int32_t price) {
        auto key = std::make_pair(price, Side);
        auto it = limit_lookup_.find(key);
        if (it == limit_lookup_.end()) {
            auto* new_limit = new MapLimit(price);
            get_book_side<Side>()[price] = new_limit;
            new_limit->side_ = Side;
            limit_lookup_[key] = new_limit;
            return new_limit;
        }
        return it->second;
    }

public:
    MapBookSide<true>::MapType bids_;
    MapBookSide<false>::MapType offers_;
    OpenAddressTable<MapOrder> order_lookup_;
    std::chrono::system_clock::time_point current_message_time_;

    double vwap_, sum1_, sum2_;
    float skew_, bid_depth_, ask_depth_;
    int32_t bid_vol_, ask_vol_;
    double imbalance_;
    std::vector<int32_t> voi_history_;
    std::vector<int32_t> mid_prices_;

    Orderbook() : order_pool_(1000000), bid_count_(0), ask_count_(0) {
        bids_.get_allocator().allocate(1000);
        offers_.get_allocator().allocate(1000);
        order_lookup_.reserve(1000000);
        limit_lookup_.reserve(2000);
        voi_history_.reserve(40000);
    }

    ~Orderbook() {
        for (auto& pair : bids_) delete pair.second;
        for (auto& pair : offers_) delete pair.second;
        bids_.clear();
        offers_.clear();
        order_lookup_.clear();
        limit_lookup_.clear();
    }

    template<bool Side>
    __attribute__((always_inline))
    void add_limit_order(uint64_t id, int32_t price, uint32_t size, uint64_t unix_time) {
        MapOrder* new_order = order_pool_.get_order();
        new_order->id_ = id;
        new_order->price_ = price;
        new_order->size = size;
        new_order->side_ = Side;
        new_order->unix_time_ = unix_time;

        MapLimit* curr_limit = get_or_insert_limit<Side>(price);
        order_lookup_.insert(id, new_order);
        curr_limit->add_order(new_order);

        if constexpr (Side) {
            ++bid_count_;
        } else {
            ++ask_count_;
        }
    }

    template<bool Side>
    void remove_order(uint64_t id, int32_t price, uint32_t size) {
        auto target = *order_lookup_.find(id);
        auto curr_limit = target->parent_;
        order_lookup_.erase(id);
        curr_limit->remove_order(target);

        if (curr_limit->is_empty()) {
            get_book_side<Side>().erase(price);
            limit_lookup_.erase(std::make_pair(price, Side));
            target->parent_ = nullptr;
        }

        if constexpr (Side) --bid_count_;
        else --ask_count_;

        order_pool_.return_order(target);
    }

    template<bool Side>
    void modify_order(uint64_t id, int32_t new_price, uint32_t new_size, uint64_t unix_time) {
        auto** target_ptr = order_lookup_.find(id);
        if (!target_ptr) {
            add_limit_order<Side>(id, new_price, new_size, unix_time);
        }

        auto target = *target_ptr;
        auto prev_price = target->price_;
        auto prev_limit = target->parent_;
        auto prev_size = target->size;

        if (prev_price != new_price) {
            prev_limit->remove_order(target);
            if (prev_limit->is_empty()) {
                get_book_side<Side>().erase(prev_price);
                std::pair<int32_t, bool> key = std::make_pair(prev_price, Side);
                limit_lookup_.erase(key);
            }
            MapLimit* new_limit = get_or_insert_limit<Side>(new_price);
            target->size = new_size;
            target->price_ = new_price;
            target->unix_time_ = unix_time;
            new_limit->add_order(target);
        } else if (prev_size < new_size) {
            prev_limit->remove_order(target);
            target->size = new_size;
            target->unix_time_ = unix_time;
            prev_limit->add_order(target);
        } else {
            target->size = new_size;
            target->unix_time_ = unix_time;
        }

        //update_modify_vol<Side>(prev_price, new_price, prev_size, new_size);
    }


    __attribute__((always_inline))
    inline void calculate_vols() {
        uint32x4_t bid_vol_vec = vdupq_n_u32(0);
        uint32x4_t ask_vol_vec = vdupq_n_u32(0);

        auto bid_it = bids_.begin();
        auto bid_end = bids_.end();
        auto ask_it = offers_.begin();
        auto ask_end = offers_.end();

        for (int i = 0; i < 100 && (bid_it != bid_end || ask_it != ask_end); i += 4) {
            uint32_t bid_chunk[4] = {0};
            uint32_t ask_chunk[4] = {0};

            for (int j = 0; j < 4; ++j) {
                if (bid_it != bid_end) {
                    bid_chunk[j] = bid_it->second->volume_;
                    ++bid_it;
                }
                if (ask_it != ask_end) {
                    ask_chunk[j] = ask_it->second->volume_;
                    ++ask_it;
                }
            }
            bid_vol_vec = vaddq_u32(bid_vol_vec, vld1q_u32(bid_chunk));
            ask_vol_vec = vaddq_u32(ask_vol_vec, vld1q_u32(ask_chunk));
        }

        bid_vol_ = vaddvq_u32(bid_vol_vec);
        ask_vol_ = vaddvq_u32(ask_vol_vec);
    }

    __attribute__((always_inline))
    inline void calculate_vwap(int32_t price, int32_t size) {
        sum1_ += static_cast<double>(price * size);
        sum2_ += static_cast<double>(size);
        vwap_ = sum1_ / sum2_;
    }

    void calculate_imbalance() {
        uint64_t total_vol = bid_vol_ + ask_vol_;
        if (total_vol == 0) {
            imbalance_ = 0.0;
            return;
        }
        imbalance_ = static_cast<double>(static_cast<int64_t>(bid_vol_) - static_cast<int64_t>(ask_vol_))
                     / static_cast<double>(total_vol);
    }


    inline void process_msg(const message &msg) {
        auto nanoseconds = std::chrono::nanoseconds(msg.time_);
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(nanoseconds);
        current_message_time_ = std::chrono::system_clock::time_point(microseconds);
        switch (msg.action_) {
            case 'A':
                msg.side_ ? add_limit_order<true>(msg.id_, msg.price_, msg.size_, msg.time_)
                          : add_limit_order<false>(msg.id_, msg.price_, msg.size_, msg.time_);
                break;
            case 'C':
                msg.side_ ? remove_order<true>(msg.id_, msg.price_, msg.size_)
                          : remove_order<false>(msg.id_, msg.price_, msg.size_);
                break;
            case 'M':
                msg.side_ ? modify_order<true>(msg.id_, msg.price_, msg.size_, msg.time_)
                          : modify_order<false>(msg.id_, msg.price_, msg.size_, msg.time_);
                break;

        }

    }

    __attribute__((always_inline))
    int32_t get_best_bid_price() const { return bids_.begin()->first; }

    __attribute__((always_inline))
    int32_t get_best_ask_price() const { return offers_.begin()->first; }

    __attribute__((always_inline))
    int32_t get_mid_price() const {
        return (get_best_bid_price() + get_best_ask_price()) / 2;
    }

    uint64_t get_count() const { return bid_count_ + ask_count_; }
};