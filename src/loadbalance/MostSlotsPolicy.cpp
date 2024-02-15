#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::map<std::string, faabric::HostResources> MostSlotsPolicy::dispatch(std::map<std::string, faabric::HostResources>& host_resources)
{
    throw std::runtime_error("MostSlotsPolicy::dispatch not implemented");

    // Sort map in-place by available slots descending
    std::sort(host_resources.begin(), host_resources.end(), [](const auto &a, const auto &b) {
        int available_a = a.second.slots() - a.second.usedslots();
        int available_b = b.second.slots() - b.second.usedslots();
        return available_a > available_b;
    });
}