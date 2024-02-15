#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::vector<std::pair<std::string, faabric::HostResources>> LeastLoadAveragePolicy::dispatch(std::vector<std::pair<std::string, faabric::HostResources>>& host_resources)
{
    throw std::runtime_error("LeastLoadAveragePolicy::dispatch not implemented");
}