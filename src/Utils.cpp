#include <iostream>
#include <string>
#include <windows.h>
#include <psapi.h>

void pinThreadToCPU()
{
    auto cpu = static_cast<int>(GetCurrentProcessorNumber());

    std::cout << "pinThreadToCPU(" << cpu  << ")=" << SetThreadAffinityMask(GetCurrentThread(), 1ULL << cpu) << std::endl;

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
}

void _SetConsoleTitle(std::string& title)
{
    SetConsoleTitleA(title.c_str());
}

size_t GetCurrentMemoryUse()
{
    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
    return (size_t)info.WorkingSetSize;
}