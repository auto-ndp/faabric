#pragma once

#include <set>
#include <string>
#include <vector>
#include <faabric/scheduler/Scheduler.h>

class LoadBalancePolicy
{
    public:
        virtual std::set<std::string> dispatch(std::set<std::string> hosts, std::vector<faabric::HostResources> host_resources) = 0;
};

class FaasmDefaultPolicy : public LoadBalancePolicy
{
    public:
        std::set<std::string> dispatch(std::set<std::string> hosts, std::vector<faabric::HostResources> host_resources) override;
};

class LeastLoadAveragePolicy : public LoadBalancePolicy
{
    public:
        std::set<std::string> dispatch(std::set<std::string> hosts, std::vector<faabric::HostResources> host_resources) override;
};

class MostSlotsPolicy : public LoadBalancePolicy
{
    public:
        std::set<std::string> dispatch(std::set<std::string> hosts, std::vector<faabric::HostResources> host_resources) override;
};