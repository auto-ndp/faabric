#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::vector<std::pair<std::string, faabric::HostResources>> MostSlotsPolicy::dispatch(std::vector<std::pair<std::string, faabric::HostResources>>& host_resources)
{
    // Sort the vector by the number of available slots in descending order
    std::sort(host_resources.begin(), host_resources.end(), [](const auto &a, const auto &b) {
        int available_a = a.second.slots() - a.second.usedslots();
        int available_b = b.second.slots() - b.second.usedslots();
        return available_a > available_b;
    });

    return host_resources;
}