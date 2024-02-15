#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::map<std::string, faabric::HostResources> MostSlotsPolicy::dispatch(std::map<std::string, faabric::HostResources>& host_resources)
{
    std::vector<std::pair<std::string, faabric::HostResources>> vec(host_resources.begin(), host_resources.end());
    
    // Sort the vector by the number of available slots in descending order
    std::sort(vec.begin(), vec.end(), [](const auto &a, const auto &b) {
        int available_a = a.second.slots() - a.second.usedslots();
        int available_b = b.second.slots() - b.second.usedslots();
        return available_a > available_b;
    });


    // Convert the vector back to a map
    std::map<std::string, faabric::HostResources> sorted_hosts;
    for (const auto &pair : vec)
    {
        sorted_hosts[pair.first] = pair.second;
    }

    return sorted_hosts;
}