#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::set<std::string> FaasmDefaultPolicy::dispatch(std::set<std::string> hosts, std::vector<faabric::HostResources> host_resources)
{
    return hosts; // Simply return the hosts in the order they were given (round-robin)
}