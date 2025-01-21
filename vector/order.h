#pragma once
#include <cstdint>

class Vector_Limit;

class Order {
public:
    Order()= default;

    Order(uint64_t order_id, int32_t price, uint32_t order_size, bool order_side, uint64_t order_unix_time) {
        id_ = order_id;
        price_ = price;
        size_ = order_size;
        side_ = order_side;
        unix_time_ = order_unix_time;
        filled_ = false;
        parent_ = nullptr;
    };

    uint64_t id_{};
    uint32_t price_{};
    uint32_t size_{};
    bool side_{};
    uint64_t unix_time_{};
    bool filled_{};
    Vector_Limit* parent_;
};