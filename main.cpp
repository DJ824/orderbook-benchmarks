#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <thread>
#include <mach/mach_time.h>
#include "vector/orderbook.cpp"
#include "parser.cpp"
#include "map/map_orderbook.cpp"

using namespace std::chrono;

void process_vector_orderbook(const std::string& filepath) {
    Parser parser(filepath);
    Vector_Orderbook orderbook;

    auto parse_start = high_resolution_clock::now();
    parser.parse();
    auto parse_end = high_resolution_clock::now();
    auto parse_duration = duration_cast<milliseconds>(parse_end - parse_start);

    std::cout << "Parsed " << parser.get_message_count() << " messages in "
              << parse_duration.count() << "ms\n";

    auto process_start = high_resolution_clock::now();
    size_t msg_count = parser.get_message_count();

    for (size_t i = 0; i < msg_count; ++i) {
        const auto& msg = parser.message_stream_[i];
        orderbook.process_msg(msg);


    }

    auto process_end = high_resolution_clock::now();
    auto process_duration = duration_cast<milliseconds>(process_end - process_start);

    std::cout << "Total processing time: " << process_duration.count() << "ms\n";

}

void process_map_orderbook(const std::string& filepath) {
    Parser parser(filepath);
    Orderbook map_orderbook;

    auto parse_start = high_resolution_clock::now();
    parser.parse();
    auto parse_end = high_resolution_clock::now();
    auto parse_duration = duration_cast<milliseconds>(parse_end - parse_start);

    std::cout << "Parsed " << parser.get_message_count() << " messages in "
              << parse_duration.count() << "ms\n";

    auto process_start = high_resolution_clock::now();
    size_t msg_count = parser.get_message_count();

    for (size_t i = 0; i < msg_count; ++i) {
        const auto &msg = parser.message_stream_[i];
        map_orderbook.process_msg(msg);

    }

    auto process_end = high_resolution_clock::now();
    auto process_duration = duration_cast<milliseconds>(process_end - process_start);

    std::cout << "Total processing time: " << process_duration.count() << "ms\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <orderbook_type>\n";
        std::cerr << "orderbook_type: 'vector' or 'map'\n";
        return 1;
    }

    std::string filepath = argv[1];
    std::string orderbook_type = argv[2];

    try {
        if (orderbook_type == "vector") {
            process_vector_orderbook(filepath);
        }
        else if (orderbook_type == "map") {
            process_map_orderbook(filepath);
        }
        else {
            std::cerr << "Invalid orderbook type. Use 'vector' or 'map'\n";
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}