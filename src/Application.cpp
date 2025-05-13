#include <windows.h>

#undef max
#undef min

#include <iostream>
#include <vector>

#if defined(NDEBUG)
#undef NDEBUG
#include <assert.h>
#define NDEBUG
#else
#include <assert.h>
#endif

#pragma comment(lib, "libs/abseil-cpp.lib")

#define RUN_TEST(expression) assert(expression); std::cout << "OK, "<< #expression << std::endl

extern void GetDataSet(const std::string& filename, std::vector<uint64_t>& data_set, uint32_t TypeSequential, bool bShuffle);
extern void GenerateDataSet(const std::string& filename, uint64_t data_set_size);
std::string Version(uint32_t Build, bool bPrint = true);
extern void pinThreadToCPU();
extern void _SetConsoleTitle(std::string& title);
extern size_t GetCurrentMemoryUse();

#include "BenchHash.h"

struct separate_thousands : std::numpunct<char> {
    char_type do_thousands_sep() const override { return '.'; }  // separate with commas
    string_type do_grouping() const override { return "\3"; } // groups of 3 digit
};

std::vector<uint64_t> data_set;

auto defaultLoc = std::cout.getloc();

template<bool bFix>
void TEST1()
{
    using namespace std::chrono_literals;
   
    //MZ::SimdHash::Index<uint64_t, MZ::SimdHash::Hash<uint64_t>, MZ::SimdHash::Mode::Fast, bFix> sh;
    //MZ::SimdHash::Index<uint64_t, absl::Hash<uint64_t>, MZ::SimdHash::Mode::Fast, bFix> sh;
    MZ::SimdHash::Index<uint64_t, absl::Hash<uint64_t>, MZ::SimdHash::Mode::ResizeOnlyEmpty, bFix> sh;

    auto mem = GetCurrentMemoryUse();

    sh.Resize(120'000'000);

    std::cout << std::endl;
    std::cout << "MZ::SimdHash::Index<uint64_t>.Resize(80'000'000) +" << (GetCurrentMemoryUse() - mem) / 1024 / 1024 << "mb, capacity=" << sh.Capacity() << std::endl;
    std::cout << std::endl;

    uint32_t step = 10'000'000, num = static_cast<uint32_t>(data_set.size() / step);

    num = 64;

    for (uint32_t i = 0; i < num; i++)
    {
        sh.Clear();

        auto memAdd = GetCurrentMemoryUse();

        auto t_start = std::chrono::high_resolution_clock::now();

        for (uint32_t j = 0; j < ((i + 1) * step); j++)
        {
            //sh.Add<true>(data_set[j]);
            sh.Add<true>(j);
        }

        auto timeAdd = (std::chrono::high_resolution_clock::now() - t_start) / 1.0s;
        
        std::cout << std::setw(11);

        std::cout << sh.Count() << ", add=" << std::setw(5) << std::fixed << std::setprecision(3) << timeAdd << 's';
        
        std::string memAddString = '+' + std::to_string((GetCurrentMemoryUse() - memAdd) / 1024 / 1024);
        std::cout << ',' << std::setw(5) << memAddString << "mb";

        std::string memString = std::to_string((GetCurrentMemoryUse() - mem) / 1024 / 1024);
        std::cout << ',' << std::setw(6) << memString << "mb";

        std::cout << ", cap=" << sh.Capacity();

        std::cout << std::endl;
    }    
}

template <typename TKey>
void TEST()
{
    TKey value = 0;

    std::cout << std::endl;
    
    MZ::SimdHash::Map<TKey, TKey, MZ::SimdHash::Hash<TKey>> hm;
    std::cout << "<<< " << typeid(hm).name() << " >>>" << std::endl;

    RUN_TEST(hm.Add(1, 1));
    RUN_TEST(!hm.Add(1, 1));

    RUN_TEST(hm.Contains(1));
    RUN_TEST(hm.TryGetValue(1, value) && value == 1);

    RUN_TEST(hm.Add(2, 2));
    RUN_TEST(hm.Contains(2));
    RUN_TEST(hm.TryGetValue(2, value) && value == 2);

    RUN_TEST(!hm.Add(2, 2));

    RUN_TEST(hm.AddOrUpdate(3, 3));
    RUN_TEST(hm.Contains(3));
    RUN_TEST(hm.TryGetValue(3, value) && value == 3);

    RUN_TEST(!hm.AddOrUpdate(3, 33));
    RUN_TEST(hm.Contains(3));
    RUN_TEST(hm.TryGetValue(3, value) && value == 33);

    RUN_TEST(hm.Remove(1));
    RUN_TEST(!hm.Contains(1));
    RUN_TEST(!hm.TryGetValue(1, value));

    RUN_TEST(!hm.Remove(1));
    RUN_TEST(!hm.Contains(1));
    RUN_TEST(hm.Add(1, 1));
    RUN_TEST(hm.Contains(1));

    hm.Add<true>(11, 11);
    std::cout << "hm.AddUnique(11, 11)" << std::endl;
    RUN_TEST(hm.Contains(11));
    RUN_TEST(hm.TryGetValue(11, value) && value == 11);
    
    std::cout << "for (const auto& x : hm) [" << hm.Count() << "] = {";

    for (const auto x : hm)
    {
        std::cout << '{' << x.key << ", " << x.value << '}' << ", ";
    }

    std::cout << "\b\b" << '}' << std::endl;
    MZ::SimdHash::Index<TKey, MZ::SimdHash::Hash<TKey>> hi;
    std::cout << "<<< " << typeid(hi).name() << " >>>" << std::endl;

    uint32_t index;

    RUN_TEST(hi.Add(1));
    RUN_TEST(hi.Contains(1));
    RUN_TEST(hi.TryGetIndex(1, index) && index == 0);

    RUN_TEST(hi.Add(2));
    RUN_TEST(hi.Contains(1));
    RUN_TEST(hi.TryGetIndex(2, index) && index == 1);

    RUN_TEST(!hi.Add(2));

    hi.Add<true>(3);
    std::cout << "hi.AddUnique(3)" << std::endl;
    RUN_TEST(hi.Contains(3));
    RUN_TEST(hi.TryGetIndex(3, index) && index == 2);

    RUN_TEST(hi.TryAdd(4, index) && index == 3);
    RUN_TEST(hi.TryGetIndex(4, index) && index == 3);
    RUN_TEST(hi.Contains(4));

    RUN_TEST(!hi.TryAdd(4, index) && index == 3);
    RUN_TEST(!hi.Contains(5));

    std::cout << "for (const auto& x : hi) [" << hi.Count() << "] = {";

    for (const auto& key : hi)
    {
        std::cout << key <<  ", ";
    }

    std::cout << "\b\b" << '}' << std::endl;
    MZ::SimdHash::Set<TKey, MZ::SimdHash::Hash<TKey>> hs;
    std::cout << "<<< " << typeid(hs).name() << " >>>" << std::endl;

    RUN_TEST(hs.Add(1));
    RUN_TEST(hs.Contains(1));

    RUN_TEST(hs.Add(2));
    RUN_TEST(hs.Contains(2));

    RUN_TEST(hs.Add(2222));
    RUN_TEST(!hs.Add(2222));

    RUN_TEST(!hs.Contains(2002));
    RUN_TEST(hs.Add(2002));
    RUN_TEST(hs.Contains(2002));

    hs.Add<true>(3);
    std::cout << "ht.AddUnique(3)" << std::endl;
    RUN_TEST(hs.Contains(3));

    RUN_TEST(hs.Remove(3));
    RUN_TEST(!hs.Contains(3));
    RUN_TEST(hs.Add(3));
    RUN_TEST(hs.Contains(3));

    std::cout << "for (const auto& x : hs) [" << hs.Count() << "] = {";

    for (const auto& key : hs)
    {
        std::cout << key << ", ";
    }

    std::cout << "\b\b" << '}' << std::endl;
}

LONG WINAPI HandleoException(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
    EXCEPTION_RECORD* record = ExceptionInfo->ExceptionRecord;

    switch (record->ExceptionCode) {
    case STATUS_INTEGER_DIVIDE_BY_ZERO:
        std::cerr << "INTEGER_DIVIDE_BY_ZERO detected!" << std::endl;
        break;
    case STATUS_ACCESS_VIOLATION:
        std::cerr << "ACCESS_VIOLATION detected!" << std::endl;
        break;
    default:
        std::cerr << "Exception Occurred, ExceptionCode=0x" << std::hex << record->ExceptionCode << std::endl;
        break;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

std::vector<std::string> cmds = { "help", "run", "rnd", "selftest", "selftest1"};

extern std::vector<int> vector_data_set_size;

int main(int argc, char** argv)
{
    SetUnhandledExceptionFilter(HandleoException);

    auto cmdIterator = std::find(cmds.begin(), cmds.end(), (argc > 1) ? argv[1] : cmds[0]);

    auto cmd = (cmdIterator == cmds.end()) ? cmds[0] : *cmdIterator;

    auto tableSizeIterator = std::find(vector_data_set_size.begin(), vector_data_set_size.end(), (argc < 3) ? 128 : atoi(argv[2]));

    bool bAddUnique = false, bShuffle = false;
    
    uint32_t TypeMask = 0, FlagMask = 0x00000000, TypeSequential = 0;
    
    MZ::SimdHash::Mode SimdHashMemoryMode = MZ::SimdHash::Mode::Fast;
    
    uint64_t BenchFlags = 0;

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-fast") == 0)
        {
            SimdHashMemoryMode = MZ::SimdHash::Mode::Fast;
        }

        if (strcmp(argv[i], "-fastdivmod") == 0)
        {
            SimdHashMemoryMode = MZ::SimdHash::Mode::FastDivMod;
        }

        if (strcmp(argv[i], "-savememoryfast") == 0)
        {
            SimdHashMemoryMode = MZ::SimdHash::Mode::SaveMemoryFast;
        }

        if (strcmp(argv[i], "-savememoryopt") == 0)
        {
            SimdHashMemoryMode = MZ::SimdHash::Mode::SaveMemoryOpt;
        }

        if (strcmp(argv[i], "-savememorymax") == 0)
        {
            SimdHashMemoryMode = MZ::SimdHash::Mode::SaveMemoryMax;
        }

        if (strcmp(argv[i], "-ankerlhash") == 0)
        {
            BenchFlags |= 8;
        }

        if (strcmp(argv[i], "-abslhash") == 0)
        {
            BenchFlags |= 4;
        }

        if (strcmp(argv[i], "-stdhash") == 0)
        {
            BenchFlags |= 2;
        }

        if (strcmp(argv[i], "-simdhash") == 0)
        {
            BenchFlags |= 1;
        }

        std::string check(argv[i]);

        if (check.find("-presult") != check.npos)
        {
            if (strcmp(argv[i], "-presult") == 0)
            {
                generate_python_data = 1;
            }
            else
            {
                generate_python_data = std::atoi(check.c_str() + 8);
            }
        }

        if (strcmp(argv[i], "-simdhm") == 0)
        {
            TypeMask |= 1; continue;
        }

        if (strcmp(argv[i], "-simdhi") == 0)
        {
            TypeMask |= 2; continue;
        }

        if (strcmp(argv[i], "-simdhs") == 0)
        {
            TypeMask |= 4; continue;
        }

        if (strcmp(argv[i], "-tslrm") == 0)
        {
            TypeMask |= 8; continue;
        }

        if (strcmp(argv[i], "-abslfhm") == 0)
        {
            TypeMask |= 16; continue;
        }

        if (strcmp(argv[i], "-em7hm") == 0)
        {
            TypeMask |= 32; continue;
        }

        if (strcmp(argv[i], "-ankerlhm") == 0)
        {
            TypeMask |= 64; continue;
        }

        if (strcmp(argv[i], "-em8hm") == 0)
        {
            TypeMask |= 128; continue;
        }

        if (strcmp(argv[i], "-simdhmfix") == 0)
        {
            TypeMask |= 2048; continue;
        }

        if (strcmp(argv[i], "-all") == 0)
        {
            TypeMask = 4096 - 1; continue;
        }

        if (strcmp(argv[i], "-32") == 0)
        {
            FlagMask |= 0x1000; continue;
        }

        if (strcmp(argv[i], "-fix") == 0)
        {
            FlagMask |= 0x2000; continue;
        }

        if (strcmp(argv[i], "-seq") == 0)
        {
            TypeSequential = 1; continue;
        }

        if (strcmp(argv[i], "-rseq") == 0)
        {
            TypeSequential = 2; continue;
        }

        if (strcmp(argv[i], "-mseq") == 0)
        {
            TypeSequential = 3; continue;
        }

        if (strcmp(argv[i], "-shuffle") == 0)
        {
            bShuffle = true; continue;
        }

        if (strcmp(argv[i], "-unique") == 0)
        {
            BenchFlags |= 0x1000'0000'0000'0000; continue;
        }

        if (strcmp(argv[i], "-rmax") == 0)
        {
            BenchFlags |= 0x0100'0000'0000'0000; continue;
        }

        if (strcmp(argv[i], "-rmin") == 0)
        {
            BenchFlags |= 0x0010'0000'0000'0000; continue;
        }

        if (strcmp(argv[i], "-ravg") == 0)
        {
            BenchFlags |= 0x0001'0000'0000'0000; continue;
        }

        if (strcmp(argv[i], "-reuse") == 0)
        {
            BenchFlags |= 0x0000'1000'0000'0000; continue;
        }

        if (strcmp(argv[i], "-test2") == 0)
        {
            BenchFlags |= 0x0000'0100'0000'0000; continue;
        }

        if (strcmp(argv[i], "-test3") == 0)
        {
            BenchFlags |= 0x0000'0010'0000'0000; continue;
        }

        if (strcmp(argv[i], "-test4") == 0)
        {
            BenchFlags |= 0x0000'0001'0000'0000; continue;
        }

        if (strcmp(argv[i], "-test5") == 0)
        {
            BenchFlags |= 0x0000'0000'1000'0000; continue;
        }
    }

    if (!(BenchFlags & 0xFF)) BenchFlags |= 4;


    switch (TypeSequential)
    {
    case 0:
        generate_python_data_mode = "rnd";
        break;
    case 1:
        generate_python_data_mode = "seq";
        break;
    case 2:
        generate_python_data_mode = "rseq";
        break;
    case 3:
        generate_python_data_mode = "mseq";
        break;
    default:
        assert(false);
        break;
    }

    if (bShuffle)
    {
        generate_python_data_mode += "/shuffle";
    }

    Version(MZ::SimdHash::Build);

    if (cmd == "help" || (TypeMask == 0 && cmd != "rnd" && cmd.find("selftest") == cmd.npos))
    {
        std::cout << std::endl;
        std::cout << "SimdHash.exe run [max [min [step]]] [-simdhm|-simdhs|-simdhi|-abslfhm|-em7hm|-em8hm]" << std::endl;
        std::cout << std::endl;

        std::cout << "-simdhm MZ::SimdHash::Map, -simdhs MZ::SimdHash::Set, -simdhi MZ::SimdHash::Index" << std::endl;
        std::cout << "-tslrs tsl::robin_set, -tslrm tsl::robin_man" << std::endl;
        std::cout << "-abslfhs absl::flat_hash_set, -abslfhm absl::flat_hash_map" << std::endl;
        std::cout << "-em7hm emhash7::Map, -em8hm emhash8::HashMap" << std::endl;
        std::cout << "-ankerlhm ankerl::unordered_dense::map" << std::endl;

        std::cout << "-stdhash, -abslhash, -simdhash, -ankerlhash, -abslhash by default" << std::endl;

        std::cout << "-reuse, by default create a new..." << std::endl;
        std::cout << "-rmin, -ravg, -rmax, call reserve before use" << std::endl;

        std::cout << "-32, -64 key size in bits, -64 by default" << std::endl;
        std::cout << "-unique ::AddUnique, by default ::Add" << std::endl;
        std::cout << "-seq, sequential set of numbers" << std::endl;
        std::cout << "-shuffle, random shuffle" << std::endl;
        std::cout << "-presult, results for python to std::cerr" << std::endl;

        std::cout << "-test1, only add/insert/emplace" << std::endl;
        std::cout << "-test2, remove(2%+2%), contains(4%+4%), add(2%+2%)" << std::endl;
        std::cout << "-test3, contains(15% load)" << std::endl;
        std::cout << "-test4, contains(15% load reverse)" << std::endl;
        std::cout << "-test5, const iterator" << std::endl;

        std::cout << std::endl;

        std::cout << "SimdHash.exe rnd [32|64|128|256]" << std::endl;
        std::cout << "32|64|128|256  dataset size in MB, 128 by default" << std::endl;

        std::cout << std::endl;

        std::cout << "SimdHash.exe selftest" << std::endl;
        std::cout << "run set of internal tests" << std::endl;

        std::cout << std::endl;

        return 0;
    }

    std::cout.imbue(std::locale(std::cout.getloc(), new separate_thousands));

    std::string filename = "SimdHash.dat";

    if (cmd == "rnd")
    {
        auto data_set_size = (vector_data_set_size.end() == tableSizeIterator) ? (128 * 1024 * 1024) : *tableSizeIterator * 1024 * 1024;

        uint8_t mode = (argc > 3 && atoi(argv[3]) >= 0 && atoi(argv[3]) < 5) ? atoi(argv[3]) : 0;

        GenerateDataSet(filename, (uint64_t)data_set_size);

        std::cout << std::endl;

        return 0;
    }

    if (cmd == "selftest")
    {
        TEST<uint64_t>();
        TEST<uint32_t>();
        return 0;
    }

    GetDataSet(filename, data_set, TypeSequential, bShuffle);

    if (cmd == "selftest1")
    {
        if (FlagMask & 0x2000)
            TEST1<true>();
        else
            TEST1<false>();

        return 0;
    }

    uint64_t maxLoad = data_set.size() - test_vector_size, startLoad = 1'000'000, stepLoad = 1'000'000;

    maxLoad = (maxLoad / 1024 / 1024);

    if (argc > 2 && argv[2][0] != '-' && atoi(argv[2]) >= 1 && atoi(argv[2]) <= maxLoad)
    {
        maxLoad = atoi(argv[2]);
    }

    if (argc > 3 && argv[3][0] != '-' && atof(argv[3]) > 0 && atoi(argv[3]) <= maxLoad)
    {
        startLoad = (uint64_t)(atof(argv[3]) * 1'000'000);
    }

    if (argc > 4 && argv[4][0] != '-' && atof(argv[4]) > 0 && atof(argv[4]) <= 10)
    {
        stepLoad = (uint64_t)(atof(argv[4]) * 1'000'000);
    }

    maxLoad = maxLoad * 1'000'000;

    if (TypeSequential > 0)
    {
        if (!bShuffle)
            std::cout << "data_set: sequential set of numbers, maxLoad: " << maxLoad << std::endl;
        else
            std::cout << "data_set: sequential set of numbers with random shuffle, maxLoad: " << maxLoad << std::endl;
    }
    else
    {
        if (!bShuffle)
            std::cout << "data_set: random set of numbers, maxLoad: " << maxLoad << std::endl;
        else
            std::cout << "data_set: random set of numbers with random shuffle, maxLoad: " << maxLoad << std::endl;
    }

    pinThreadToCPU();

    std::cout << std::endl;

    InitTestVector(data_set);

    for (uint32_t iType = 1; iType <= 4096; iType <<= 1)
    {
        if ((iType & TypeMask) == 0) continue;
        
        switch ((iType & TypeMask) | FlagMask)
        {
        case 0x0001:
        {
            switch (SimdHashMemoryMode)
            {
            case  MZ::SimdHash::Mode::FastDivMod:
                    BenchSimdHashMap<uint64_t, MZ::SimdHash::Mode::FastDivMod, false>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
                break;
            case  MZ::SimdHash::Mode::SaveMemoryFast:
                    BenchSimdHashMap<uint64_t, MZ::SimdHash::Mode::SaveMemoryFast, false>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
                    break;
            case  MZ::SimdHash::Mode::SaveMemoryOpt:
                    BenchSimdHashMap<uint64_t, MZ::SimdHash::Mode::SaveMemoryOpt, false>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
                    break;
            case  MZ::SimdHash::Mode::SaveMemoryMax:
                    BenchSimdHashMap<uint64_t, MZ::SimdHash::Mode::SaveMemoryMax, false>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
                    break;
                default:
                    BenchSimdHashMap<uint64_t, MZ::SimdHash::Mode::Fast, false>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
                    break;
            }
        }
        break;
        case 0x2001:
        {
            switch (SimdHashMemoryMode)
            {
            case  MZ::SimdHash::Mode::FastDivMod:
                BenchSimdHashMap<uint64_t, MZ::SimdHash::Mode::FastDivMod, true>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
                break;
            case  MZ::SimdHash::Mode::SaveMemoryFast:
                BenchSimdHashMap<uint64_t, MZ::SimdHash::Mode::SaveMemoryFast, true>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
                break;
            case  MZ::SimdHash::Mode::SaveMemoryOpt:
                BenchSimdHashMap<uint64_t, MZ::SimdHash::Mode::SaveMemoryOpt, true>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
                break;
            case  MZ::SimdHash::Mode::SaveMemoryMax:
                BenchSimdHashMap<uint64_t, MZ::SimdHash::Mode::SaveMemoryMax, true>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
                break;

            default:
                BenchSimdHashMap<uint64_t, MZ::SimdHash::Mode::Fast, true>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
                break;
            }
        }
        break;
        case 0x1001:
        {
            BenchSimdHashMap<uint32_t, MZ::SimdHash::Mode::Fast, false>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x3001:
        {
            BenchSimdHashMap<uint32_t, MZ::SimdHash::Mode::Fast, true>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x0002:
        {
            BenchSimdHashIndex<uint64_t, false>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x2002:
        {
            BenchSimdHashIndex<uint64_t, true>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x1002:
        {
            BenchSimdHashIndex<uint32_t, false>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x3002:
        {
            BenchSimdHashIndex<uint32_t, true>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x0004:
        {
            BenchSimdHashSet<uint64_t, false>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x2004:
        {
            BenchSimdHashSet<uint64_t, true>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x1004:
        {
            BenchSimdHashSet<uint32_t, false>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x3004:
        {
            BenchSimdHashSet<uint32_t, true>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x0008:
        case 0x2008:
        {
            BenchTslRobinMap<uint64_t>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x1008:
        case 0x3008:
        {
            BenchTslRobinMap<uint32_t>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x0010:
        case 0x2010:
        {
            BenchAbslFlatHashMap<uint64_t>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x1010:
        case 0x3010:
        {
            BenchAbslFlatHashMap<uint32_t>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x0020:
        case 0x2020:
        {
            BenchEm7HashMap<uint64_t>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x1020:
        case 0x3020:
        {
            BenchEm7HashMap<uint32_t>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x0040:
        case 0x2040:
        {
            BenchAnkerlHashMap<uint64_t>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x1040:
        case 0x3040:
        {
            BenchAnkerlHashMap<uint32_t>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x0080:
        case 0x2080:
        {
            BenchEm8HashMap<uint64_t>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x1080:
        case 0x3080:
        {
            BenchEm8HashMap<uint32_t>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x0800:
        case 0x2800: //simdhmfix
        {
            BenchSimdHashMap<uint64_t, MZ::SimdHash::Mode::Fast, true>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        case 0x1800:
        case 0x3800: //simdhmfix
        {
            BenchSimdHashMap<uint32_t, MZ::SimdHash::Mode::Fast, true>(BenchFlags, data_set, startLoad, maxLoad, stepLoad);
        }
        break;
        }
        std::cout << std::endl;
    }
}
