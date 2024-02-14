#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::vector<std::string> MostSlotsPolicy::dispatch(const std::set<std::string>& warm_faaslets, int number_of_messages, faabric::scheduler::Scheduler& scheduler)
{
    throw std::runtime_error("MostSlotsPolicy::dispatch not implemented");
}