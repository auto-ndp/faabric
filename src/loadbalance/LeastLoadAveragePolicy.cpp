#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::string LeastLoadAveragePolicy::dispatch(const std::set<std::string>& warm_faaslets)
{
    throw std::runtime_error("LeastLoadAveragePolicy::dispatch not implemented");
}