#include <faabric/util/system_metrics.h>
namespace faabric::util {

    CPUStats getCPUUtilisation() {
        std::ifstream cpuinfo("/proc/stat");
        std::string line;
        if (!cpuinfo.is_open()) {
          throw std::runtime_error("Unable to open /proc/stat");
        }   
        std::getline(cpuinfo, line);
        // Extract CPU utilization information from the line
        std::istringstream iss(line);
        std::string cpuLabel;
        long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;  
        iss >> cpuLabel >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;  
        // Calculate total CPU time
        long totalCpuTime = user + nice + system + idle + iowait + irq + softirq + steal + guest + guest_nice;
        CPUStats stats;
        stats.totalCpuTime = totalCpuTime;
        stats.idleCpuTime = idle;
        return stats;
    }

    double getMemoryUtilisation()
    {
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        if (!meminfo.is_open()) {
          throw std::runtime_error("Unable to open /proc/meminfo");
        }   
        std::getline(meminfo, line);
        std::istringstream ss(line);
        std::string mem;
        ss >> mem;  
        uint64_t totalMem;
        ss >> totalMem; 
        std::getline(meminfo, line);
        ss = std::istringstream(line);
        ss >> mem;  
        uint64_t availableMem;
        ss >> availableMem; 
        return 1.0 - (availableMem / (double)totalMem);
    }

    double getLoadAverage()
    {
      std::ifstream loadavg("/proc/loadavg");
      std::string line;
      if (!loadavg.is_open()) {
        throw std::runtime_error("Unable to open /proc/loadavg");
      }

      std::getline(loadavg, line);
      std::istringstream ss(line);
      double load1, load5, load15;
      ss >> load1 >> load5 >> load15;
      return load1;
    }

    UtilisationStats getSystemUtilisation()
    {
        UtilisationStats stats; 

        // Get initial figures
        CPUStats cpuStart = getCPUUtilisation();    
        std::this_thread::sleep_for(std::chrono::milliseconds(1));  

        // Get final figures
        CPUStats cpuEnd = getCPUUtilisation();
        long cpuTimeDelta = cpuEnd.totalCpuTime - cpuStart.totalCpuTime;
        long idleTimeDelta = cpuEnd.idleCpuTime - cpuStart.idleCpuTime;
        double cpu_utilisation = 1.0 - (idleTimeDelta / (double) cpuTimeDelta); 
        stats.cpu_utilisation = cpu_utilisation;
        stats.ram_utilisation = getMemoryUtilisation();
        stats.load_average = getLoadAverage();  
        return stats;
    }




}


