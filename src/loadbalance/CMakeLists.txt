faabric_lib(loadbalance
    FaasmDefaultPolicy.cpp
    LeastLoadAveragePolicy.cpp
    MostSlotsPolicy.cpp)


target_link_libraries(loadbalance PRIVATE faabric::scheduler)