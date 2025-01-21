cmake_minimum_required(VERSION 3.27)
project(vector_ob)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3")

find_path(XXHASH_INCLUDE_DIR xxhash.h
        PATHS /opt/homebrew/Cellar/xxhash/*/include
        /usr/local/include
        /usr/include
)

find_library(XXHASH_LIBRARY
        NAMES xxhash libxxhash
        PATHS /opt/homebrew/Cellar/xxhash/*/lib
        /usr/local/lib
        /usr/lib
)

add_executable(vector_ob
        main.cpp
        vector/order.h
        vector/orderbook.cpp
        vector/limit.h
        lookup_table.h
        vector/order_pool.h
        message.h
        parser.cpp
        map/map_order.cpp
        map/map_limit.cpp
        map/map_order_pool.cpp
        map/map_orderbook.cpp
)

target_include_directories(vector_ob PRIVATE ${XXHASH_INCLUDE_DIR})
target_link_libraries(vector_ob PRIVATE ${XXHASH_LIBRARY})