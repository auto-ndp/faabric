#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::string MostSlotsPolicy::dispatch(const std::set<std::string>& warm_faaslets)
{
    throw std::runtime_error("MostSlotsPolicy::dispatch not implemented");
}