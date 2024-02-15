#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::map<std::string, faabric::HostResources> MostSlotsPolicy::dispatch(std::map<std::string, faabric::HostResources> host_resources)
{
    throw std::runtime_error("MostSlotsPolicy::dispatch not implemented");
}