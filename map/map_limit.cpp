#pragma once

#include <cstdint>
#include "map_order.cpp"

class MapLimit {
public:
    explicit MapLimit(int32_t price)
            : price_(price)
            , volume_(0)
            , num_orders_(0)
            , head_(nullptr)
            , tail_(nullptr)
            , side_(false)
    {}

    explicit MapLimit(MapOrder* new_order)
            : price_(new_order->price_)
            , volume_(new_order->size)
            , num_orders_(1)
            , head_(new_order)
            , tail_(new_order)
            , side_(new_order->side_)
    {}

    __attribute__((always_inline))
    inline void add_order(MapOrder* new_order) {
        if (head_ == nullptr && tail_ == nullptr) {
            head_ = new_order;
            tail_ = new_order;
        } else {
            tail_->next_ = new_order;
            new_order->prev_ = tail_;
            tail_ = tail_->next_;
            tail_->next_ = nullptr;
        }
        volume_ += new_order->size;
        ++num_orders_;
        new_order->parent_ = this;
    }

    __attribute__((always_inline))
    inline void remove_order(MapOrder* target) {
        if (!target || !head_) return;

        volume_ -= target->size;
        --num_orders_;

        if (head_ == tail_ && head_ == target) {
            head_ = nullptr;
            tail_ = nullptr;
        }
        else if (head_ == target) {
            head_ = head_->next_;
            if (head_) head_->prev_ = nullptr;
        }
        else if (tail_ == target) {
            tail_ = tail_->prev_;
            if (tail_) tail_->next_ = nullptr;
        }
        else {
            if (target->prev_) target->prev_->next_ = target->next_;
            if (target->next_) target->next_->prev_ = target->prev_;
        }

        target->next_ = nullptr;
        target->prev_ = nullptr;
        target->parent_ = nullptr;
    }

    int32_t get_price() const { return price_; }
    uint64_t get_volume() const { return volume_; }
    uint32_t get_size() const { return num_orders_; }
    bool is_empty() const { return head_ == nullptr && tail_ == nullptr; }
    uint64_t total_volume() const { return volume_; }

    void reset() {
        price_ = 0;
        volume_ = 0;
        num_orders_ = 0;
        head_ = nullptr;
        tail_ = nullptr;
    }

    void set(int32_t price) { price_ = price; }

    int32_t price_;
    uint64_t volume_;
    uint32_t num_orders_;
    MapOrder* head_;
    MapOrder* tail_;
    bool side_;
};