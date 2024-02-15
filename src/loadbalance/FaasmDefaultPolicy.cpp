#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::map<std::string, faabric::HostResources> FaasmDefaultPolicy::dispatch(std::map<std::string, faabric::HostResources> host_resources)
{
    return host_resources; // Simply return the hosts in the order they were given (round-robin)
}