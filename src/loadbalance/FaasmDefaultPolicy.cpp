#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::vector<std::pair<std::string, faabric::HostResources>> FaasmDefaultPolicy::dispatch(std::vector<std::pair<std::string, faabric::HostResources>>& host_resources)
{
    return host_resources;
}