#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::string LeastLoadAveragePolicy::dispatch(const std::set<std::string>& warm_faaslets, int number_of_messages, faabric::scheduler::Scheduler& scheduler)
{
    throw std::runtime_error("LeastLoadAveragePolicy::dispatch not implemented");
}