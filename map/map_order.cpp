#pragma once

#include <cstdint>
#include <string>
#include <ctime>

class MapLimit;

class MapOrder {
public:
    MapOrder(uint64_t order_id, int32_t price, uint32_t order_size, bool order_side, uint64_t order_unix_time)
            : id_(order_id)
            , size(order_size)
            , price_(price)
            , side_(order_side)
            , unix_time_(order_unix_time)
            , filled_(false)
            , next_(nullptr)
            , prev_(nullptr)
            , parent_(nullptr)
    {}

    MapOrder()
            : id_(0)
            , size(0)
            , price_(0)
            , side_(true)
            , unix_time_(0)
            , filled_(false)
            , next_(nullptr)
            , prev_(nullptr)
            , parent_(nullptr)
    {}

    uint64_t id_;
    int32_t price_;
    uint32_t size;
    bool side_;
    uint64_t unix_time_;
    MapOrder* next_;
    MapOrder* prev_;
    MapLimit* parent_;
    bool filled_;
};