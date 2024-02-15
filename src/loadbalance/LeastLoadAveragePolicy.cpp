#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::set<std::string> LeastLoadAveragePolicy::dispatch(std::set<std::string> hosts, std::vector<faabric::HostResources> host_resources)
{
    throw std::runtime_error("LeastLoadAveragePolicy::dispatch not implemented");
}