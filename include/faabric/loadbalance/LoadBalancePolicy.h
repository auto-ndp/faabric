#pragma once

#include <set>
#include <string>
#include <vector>
#include <faabric/scheduler/Scheduler.h>

class LoadBalancePolicy
{
    public:
        virtual std::map<std::string, faabric::HostResources> dispatch(std::map<std::string, faabric::HostResources> host_resources) = 0;
};

class FaasmDefaultPolicy : public LoadBalancePolicy
{
    public:
        std::map<std::string, faabric::HostResources> dispatch(std::map<std::string, faabric::HostResources> host_resources) override;
};

class LeastLoadAveragePolicy : public LoadBalancePolicy
{
    public:
        std::map<std::string, faabric::HostResources> dispatch(std::map<std::string, faabric::HostResources> host_resources) override;
};

class MostSlotsPolicy : public LoadBalancePolicy
{
    public:
        std::map<std::string, faabric::HostResources> dispatch(std::map<std::string, faabric::HostResources> host_resources) override;
};