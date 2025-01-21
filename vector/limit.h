#pragma once
#include "order.h"
#include <vector>

class Vector_Limit {
public:
    uint32_t volume_{0};
    uint32_t num_orders_{0};
    std::vector<Order*> orders_;

    Vector_Limit() {
        orders_.reserve(64);
    }

    __attribute__((always_inline))
    void add_order(Order* new_order) {
        orders_.push_back(new_order);
        volume_ += new_order->size_;
        ++num_orders_;
    }

    __attribute__((always_inline))
    void remove_order(Order* target) {
        auto it = std::find(orders_.begin(), orders_.end(), target);
        if (it != orders_.end()) {
            volume_ -= target->size_;
            orders_.erase(it);
            --num_orders_;
            target->parent_ = nullptr;
        } else {
            throw std::runtime_error("Attempted to remove non-existent order");
        }
    }




    __attribute__((always_inline))
    bool is_empty() const { return num_orders_ == 0; }

    __attribute__((always_inline))
    uint32_t get_volume() const { return volume_; }

    __attribute__((always_inline))
    uint32_t get_order_count() const { return num_orders_; }
};