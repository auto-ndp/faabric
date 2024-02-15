#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::map<std::string, faabric::HostResources> LeastLoadAveragePolicy::dispatch(std::map<std::string, faabric::HostResources> host_resources)
{
    throw std::runtime_error("LeastLoadAveragePolicy::dispatch not implemented");
}