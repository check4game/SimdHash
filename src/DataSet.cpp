#include <iostream>
#include <fstream>
#include <string>
#include <random>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <numeric>

using namespace std::chrono;

std::vector<int> vector_data_set_size = { 32, 64, 128, 256 };

std::vector<uint64_t> buffer(128 * 1024);

#include "Judy.h"

#pragma comment(lib, "libs/Judy.lib")

void GetDataSet(const std::string& filename, std::vector<uint64_t>& data_set, uint32_t TypeSequential, bool bShuffle)
{
    std::cout << "load from '" << filename << "'";

    std::ifstream inFile;

    inFile.open(filename, std::ios_base::binary);

    if (!inFile.is_open())
    {
        std::cerr << std::endl << "Failed to open file for reading." << std::endl;
        exit(-1);
    }

    auto t_start = high_resolution_clock::now();

    uint64_t data_set_size;

    inFile.read(reinterpret_cast<char*>(&data_set_size), sizeof(data_set_size));
    
    auto it = std::find(vector_data_set_size.begin(), vector_data_set_size.end(), (data_set_size - (1024ULL * 1024 * 2)) / 1024 / 1024);

    if (it == vector_data_set_size.end() || ((data_set_size - (1024ULL * 1024)) % 1024) != 0)
    {
        std::cerr << std::endl << "Failed read data, file corrupted..." << std::endl;
        exit(-1);
    }

    data_set.resize(data_set_size);

    if (TypeSequential > 0)
    {
        switch (TypeSequential)
        {
        case 2:
            {
                auto start_value = ~0ULL;

                std::generate(data_set.begin(), data_set.end(),
                    [&start_value]()
                    {
                        return start_value--;
                    });
                break;
            }
        case 3:
            std::iota(data_set.begin(), data_set.end(), ((~0ULL) / 2 - data_set_size / 2 - 1));
            break;
        default:
            std::iota(data_set.begin(), data_set.end(), 0ULL);
            break;

        }
    }
    else
    {
        auto bufferSize = (128LLU * 1024 / sizeof(uint64_t));

        for (uint64_t i = 0; i < data_set_size; i += bufferSize)
        {
            if (!inFile.read(reinterpret_cast<char*>(data_set.data() + i), bufferSize * sizeof(uint64_t)))
            {
                std::cerr << std::endl << "Failed read data, file corrupted..." << std::endl;
                exit(-1);
            }
        }

        inFile.close();
    }

    if (bShuffle)
    {
        std::cout << ", random shuffle ...";
        std::mt19937_64 generator(2);
        std::shuffle(data_set.begin(), data_set.end(), generator);
    }

    auto t_end = high_resolution_clock::now();

    std::cout << ", time: " << (t_end - t_start) / 1ms / 1000.0 << " s";
    std::cout << ", size: " << data_set_size;

    std::cout << std::endl;

    return;
}

void GenerateDataSet(const std::string& filename, uint64_t data_set_size)
{
    std::cout << "generate " << data_set_size << " + (2 * 1024 * 1024) random set of numbers to '" << filename << "' ";

    data_set_size += (1024ULL * 1024 * 2);

    std::ofstream outFile(filename, std::ios_base::binary | std::ios_base::trunc);

    if (!outFile.is_open()) {
        std::cerr << std::endl << "Failed to open file for writing." << std::endl;
        return;
    }

    auto t_start = high_resolution_clock::now();

    std::mt19937_64 generator(1);
    std::uniform_int_distribution<uint64_t> distribution;

    Pvoid_t JArrayMain = NULL;

    uint32_t count = 0; buffer.clear();

    const uint64_t judyMask = 0x0000'0000'FFFF'FFFF;

    buffer.push_back(data_set_size);

    while (count < data_set_size)
    {
        auto rnd = distribution(generator);

        int rc; 
        
        J1S(rc, JArrayMain, rnd & judyMask);

        if (rc != 1) continue;

        //J1S(rc, JArrayMain, rnd & ~judyMask);
        J1S(rc, JArrayMain, rnd >> 32);

        if (rc != 1) continue;

        count++; buffer.push_back(rnd);

        if (buffer.size() == buffer.capacity())
        {
            outFile.write(reinterpret_cast<const char*>(&buffer.front()), buffer.size() * sizeof(uint64_t));
            buffer.clear();
        }

        if ((count % 5'000'000) == 0)  std::cout << '.';
    }

    if (buffer.size() > 0)
    {
        outFile.write(reinterpret_cast<const char*>(&buffer.front()), buffer.size() * sizeof(uint64_t));
        buffer.clear();
    }

    outFile.flush(); outFile.close();

    auto t_end = high_resolution_clock::now();

    std::cout << std::endl << std::fixed << std::setprecision(3) << "time: " << (t_end - t_start) / 1ms / 1000.0 << " s"
        << ", size: " << count << ", MemUsed: " << Judy1MemUsed(JArrayMain) / 1024 / 1024 << " MB";

    std::cout << std::endl;

    Word_t ret; J1FA(ret, JArrayMain);
}
