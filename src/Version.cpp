#include <string>
#include <algorithm>
#include <regex>
#include <iostream>

#include "libcpuid.h"

#pragma comment(lib, "libs/libcpuid.lib")


std::string Version(uint32_t Build, bool bPrint = true)
{
    if (bPrint) std::cout << "SimdHash, build: " << Build << ", Date: " << __DATE__ << " " << __TIME__ << std::endl;

    if (cpuid_present())
    {
        struct cpu_raw_data_t raw;
        struct cpu_id_t data;

        if (cpuid_get_raw_data(&raw) >= 0 && cpu_identify(&raw, &data) >= 0)
        {
            auto brand = std::regex_replace(std::string(data.brand_str), std::regex("^\\s+|\\s+$"), "");
            auto codename = std::regex_replace(std::string(data.cpu_codename), std::regex("^\\s+|\\s+$"), "");

            if (bPrint)
            {
                std::cout << brand << ", " << codename << ", " << data.num_cores << "/" << data.num_logical_cpus
                    << ", " << cpu_clock_by_os() / 1000.0 << "GHz";

                std::cout << std::endl;
            }

            return brand;
        }
    }

    return "n/a";
}
