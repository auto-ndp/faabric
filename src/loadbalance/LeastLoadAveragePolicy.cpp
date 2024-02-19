#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::vector<std::pair<std::string, faabric::HostResources>> LeastLoadAveragePolicy::dispatch(std::vector<std::pair<std::string, faabric::HostResources>>& host_resources)
{
    // Sort the vector by the load average in ascending order
    std::sort(host_resources.begin(), host_resources.end(), [](const auto &a, const auto &b) {
        return a.second.loadaverage() < b.second.loadaverage();
    });

    return host_resources;
}