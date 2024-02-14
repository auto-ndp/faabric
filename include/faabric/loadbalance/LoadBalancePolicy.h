#pragma once

#include <set>
#include <string>   

class LoadBalancePolicy
{
    public:
        virtual std::string dispatch(const std::set<std::string>& warm_faaslets) = 0;
};

class FaasmDefaultPolicy : public LoadBalancePolicy
{
    public:
        std::string dispatch(const std::set<std::string>& warm_faaslets) override;
};

class LeastLoadAveragePolicy : public LoadBalancePolicy
{
    public:
        std::string dispatch(const std::set<std::string>& warm_faaslets) override;
};

class MostSlotsPolicy : public LoadBalancePolicy
{
    public:
        std::string dispatch(const std::set<std::string>& warm_faaslets) override;
};