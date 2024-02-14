#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::string FaasmDefaultPolicy::dispatch(const std::set<std::string>& warm_faaslets)
{
    throw std::runtime_error("FaasmDefaultPolicy::dispatch not implemented");
}