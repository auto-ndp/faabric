#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::set<std::string> MostSlotsPolicy::dispatch(std::set<std::string> hosts, std::vector<faabric::HostResources> host_resources)
{
    throw std::runtime_error("MostSlotsPolicy::dispatch not implemented");
}