# orderbook-benchmarks
benchmarking different orderbook designs 

In our implementations, the orderbook is comprised of 3 pieces, the order object which represents the individual limit order, the limit object which represents all the orders at a certain price level, and the main orderbook. The orderbook has 3 main operations, add order, modify order, and remove order. When a market order is submitted, we adjust the size of the limit order that got executed on using the modify order function, or remove order if the market order cleared all the lots on the limit order. 

benchmarking was conducted on a 32gb m1 max using clang 
add -g and -03 flags to enable optimization 

## MAP 

- in this design, we have 2 <std::map<uint32_t, Limit*, comparator>> to represent the orderbook with bids being in descending order and offers being in ascending order
- each limit object is comprised of a double linked list of order objects, and we store individual pointers to order objects in a custom open address table (another repo for statistics) by order_id for o(1) access to each limit order
- pointers to limit objects are also stored in a std::unordered_map<std::pair<int32_t, bool>, MapLimit*, boost::hash<std::pair<int32_t, bool>>> hashing the combination of price,bool to determine which side of the book the limit belongs to. thus, we have o(1) + cost of hash access to each limit object

Functions and Time Complexity 

add_order: adds a limit order to the book, runs in O(logn) worst case if we have to insert a new limit object in our maps, else O(1) + cost of hash using the limit lookup map 

modify_order: modifies an existing limit order, if the price changes or the size increases, requeues the order. runs in O(logn) worst case if we have to requeue the order and add/remove limit objects, else O(1) via the order lookup 

remove_order: removes a limit order, if it was the last limit order for the corresponding limit object, removes the limit object from the map, runs in O(logn) if limit object removal is required, else O(1). 


Performance 

<img width="349" alt="image" src="https://github.com/user-attachments/assets/77cae073-ab5e-41cf-a8db-2818e355ba98" />



## Vector
- in this design, we use vectors instead of maps to represent the sides of the book, we use a comparator function that we pass into <std::lower_bound> to get log(n) access to the book and maintain proper ordering.
- the limit objects are comprised of another <std::vector> of orders, again storing pointers to orders in a custom lookup table
- we use the same unordered_map from the map design to store pointers to limit objects as well

Functions and Time Complexity 

add_order: runs in O(logn) worst case if we have to insert a new limit object in the vector, else O(1) + cost of hash for limit lookup 

modify_order: runs in O(logn) if limit addition/removal is required, else O(1) via order lookup

remove_order: runs in O(m) where m = num of orders on the limit object (i tried doing a thing where we binary search using the unix_time as the fifo ordering would make it so the time would be sequentially increasing, but the way the data is formatted when we first process the book the first 1xxxx messages or so all have the same unix_time, which caused errors when trying to find an order, maybe i can generate my own identifiers to make it work idk) 

Performance 

<img width="380" alt="image" src="https://github.com/user-attachments/assets/763e9169-81ff-4bc6-b157-43e4e6414993" />


Unfortunately, we don't have perf on macos, but i ran the following command to enable detailed performance tracking and a time profiler: xcrun xctrace record --template 'Time Profiler' --launch  

<img width="1275" alt="image" src="https://github.com/user-attachments/assets/0af83d4b-c147-4b47-81b6-1fd8938f6947" />

first, we the remove order function, as we go down the call tree we see that <std::find> is a major bottleneck in our function, along with <std::erase>, which was to be expected with O(m) complexity for both, to make this version beat our map implementation, we would have to fix this 

in the add order function, we see that the hashing of int32_t/bool in the limit lookup table is a significant cost, maybe we can have 2 unordered_maps for lookup and just insert/erase when needed, will have to see, otherwise, the other main holdup is the cost of xxhash in our custom lookup table, which i have tested to beat <std::unordered_map>,

the modify order function is mainly comprised of order addition/removal if price change/size increase, otherwise finding the order is relatively quick 

another thing i noticed here was the constructor takes a while as well, due to the memory allocations when reserving space, want to try to write my own allocater to see how it would compare 


