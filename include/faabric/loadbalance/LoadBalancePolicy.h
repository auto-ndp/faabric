#pragma once

#include <set>
#include <string>
#include <vector>
#include <faabric/scheduler/Scheduler.h>

class LoadBalancePolicy
{
    public:
        virtual std::vector<std::string> dispatch(const std::set<std::string>& warm_faaslets, int number_of_messages, const faabric::scheduler::Scheduler& scheduler) = 0;
};

class FaasmDefaultPolicy : public LoadBalancePolicy
{
    public:
        std::vector<std::string> dispatch(const std::set<std::string>& warm_faaslets, int number_of_messages, const faabric::scheduler::Scheduler& scheduler) override;
};

class LeastLoadAveragePolicy : public LoadBalancePolicy
{
    public:
        std::vector<std::string> dispatch(const std::set<std::string>& warm_faaslets, int number_of_messages, const faabric::scheduler::Scheduler& scheduler) override;
};

class MostSlotsPolicy : public LoadBalancePolicy
{
    public:
        std::vector<std::string> dispatch(const std::set<std::string>& warm_faaslets, int number_of_messages, const faabric::scheduler::Scheduler& scheduler) override;
};