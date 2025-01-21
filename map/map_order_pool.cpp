#pragma once

#include <vector>
#include <memory>
#include "map_order.cpp"

class MapOrderPool {
public:
    explicit MapOrderPool(size_t initial_size) {
        pool_.reserve(initial_size);
        available_orders_.reserve(initial_size);

        for (size_t i = 0; i < initial_size; ++i) {
            pool_.push_back(std::make_unique<MapOrder>());
            available_orders_.push_back(pool_.back().get());
        }
    }

    __attribute__((always_inline))
    inline MapOrder* get_order() {
        if (available_orders_.empty()) {
            pool_.push_back(std::make_unique<MapOrder>());
            return pool_.back().get();
        }
        MapOrder* order = available_orders_.back();
        available_orders_.pop_back();
        return order;
    }

    __attribute__((always_inline))
    inline void return_order(MapOrder* order) {
        available_orders_.push_back(order);
    }

    __attribute__((always_inline))
    inline void reset() {
        pool_.clear();
        pool_.reserve(1000000);
    }

private:
    std::vector<std::unique_ptr<MapOrder>> pool_;
    std::vector<MapOrder*> available_orders_;
};