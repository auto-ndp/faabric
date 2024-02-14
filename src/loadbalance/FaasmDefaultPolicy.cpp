#include <faabric/loadbalance/LoadBalancePolicy.h>
#include <stdexcept>

std::vector<std::string> FaasmDefaultPolicy::dispatch(const std::set<std::string>& warm_faaslets, int number_of_messages, const faabric::scheduler::Scheduler& scheduler)
{
    std::vector<std::string> hosts_delta;
    hosts_delta.reserve(number_of_messages);
    for (auto& faaslet : warm_faaslets) {
        int remainder = number_of_messages;
        faabric::HostResources host_resources = scheduler.getResourcesForHost(faaslet);
        int availableSlots = host_resources.slots() - host_resources.usedslots();
        availableSlots = std::max(availableSlots, 0);
        int nOnThisHost = std::min<int>(availableSlots, 1);

        for (int i = 0; i < nOnThisHost; i++) {
            hosts_delta.push_back(faaslet);
        }

        remainder -= nOnThisHost;
        if (remainder <= 0) {
            break;
        }
    }
    return hosts_delta;
}