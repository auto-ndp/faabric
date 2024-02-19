
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>

namespace faabric::util {
    struct UtilisationStats
    {
      double cpu_utilisation;
      double ram_utilisation;
      double load_average;
    };

    struct CPUStats
    {
      long totalCpuTime;
      long idleCpuTime;
    };

    struct MemStats
    {
      uint64_t total;
      uint64_t available;
    };

    UtilisationStats getSystemUtilisation();
    CPUStats getCPUUtilisation();
    double getMemoryUtilisation();
    double getLoadAverage();
}