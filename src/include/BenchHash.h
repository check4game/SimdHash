#ifndef __BENCHHASH_H__
#define __BENCHHASH_H__

#include "SimdHash.h"

#include <stdint.h>
#include <chrono>
#include <regex>
#include <sstream>
#include <type_traits>

#include "tsl/robin_set.h"
#include "tsl/robin_map.h"

#define ABSL_HASH_SEED 0
#include "absl/container/flat_hash_set.h"
#include "absl/container/flat_hash_map.h"

#include "unordered_dense.h"

#include "hash_table7.hpp"
#define EMH_SIZE_TYPE 0
#include "hash_table8.hpp"

#if defined(assert)
#undef assert
#endif

#define assert(cond) \
    do {                \
        if (!(cond)) {  \
            fprintf(stderr, "%s:%d: Assertion failed in function '%s': %s\n", __FILE__, __LINE__, __FUNCSIG__, #cond); \
            abort();    \
        }               \
    } while (false)

#define HAS_METHOD(name) \
template<typename T, typename... Args> \
struct has_##name { \
    template<typename U> \
    static auto test(U*) -> decltype(std::declval<U>().name(std::declval<Args>()...), std::true_type()); \
    \
    template<typename> \
    static std::false_type test(...); \
    \
    constexpr operator bool() const { return decltype(test<T>(nullptr))::value; } \
};

HAS_METHOD(Resize)

template <typename T>
constexpr bool isSimd = has_Resize<T, uint32_t>();

HAS_METHOD(Add)
HAS_METHOD(insert)

template <typename T, typename TKey>
constexpr bool isSet = has_Add<T, TKey>() || has_insert<T, TKey>();

HAS_METHOD(TryGetIndex)

template <typename T, typename TKey>
constexpr bool isIndex = has_TryGetIndex<T, const TKey&, uint32_t&>();

constexpr float CalcLoadFactor(uint32_t value) noexcept
{
    if (value < 16) value = 16; else if (value > 1'000) value = 1'000;

    auto power = value * 1000;

    value = power + 1;

    power |= power >> 1;
    power |= power >> 2;
    power |= power >> 4;
    power |= power >> 8;
    power |= power >> 16;

    power++;

    return static_cast<float>(static_cast<double>(value) / power);
}

enum class TestType
{
    UNKNOWN = 0,
    TEST1,
    TEST2,
    TEST3,
    TEST4,
    TEST5,
};

template <typename TKey>
class BencObjecthInterface
{
public:
    virtual bool TestAdd(uint64_t* data_set, uint32_t load, bool bUnique) { return false; }

    virtual bool TestRemove(uint64_t* data_set, uint32_t load) { return false; }

    virtual bool TestContains(uint64_t* data_set, uint32_t load, bool bReverse = false) { return false; }

    virtual void Clear() {}

    virtual void Init(uint32_t reserve, bool, TestType) {}

    virtual uint32_t Count() const { return 0; }

    virtual uint32_t Capacity() const { return 0; }

    virtual float load_factor() const { return 0.0f; }

    virtual uint64_t KeysSum() { return 0; }

    virtual std::string name() { return ""; }
    virtual std::string short_name() { return ""; }

    virtual void Bench(uint64_t BenchFlags, std::stringstream& result, std::vector<uint64_t>& data_set, uint64_t load, TestType) {}
};

static int generate_python_data = 0;

static std::string generate_python_data_mode;

static size_t memory_usage_start = 0;

static std::vector<uint64_t> erase_vector;
static std::vector<uint64_t> add_vector;
static std::vector<uint64_t> find_vector;

static std::vector<uint64_t> unknown_vector;

static constexpr uint64_t test_vector_size = 1024ULL * 1024 * 2;

static void MakeTestVector(std::vector<uint64_t>& data_set, uint64_t load, uint64_t test_load)
{
    assert(load <= data_set.size());

    assert(test_load <= test_vector_size);

    erase_vector.clear();
    add_vector.clear();
    find_vector.clear();
    unknown_vector.clear();

    std::mt19937 generator(3);

    std::sample(data_set.begin(), data_set.begin() + load, std::back_inserter(erase_vector), test_load, generator);

    std::sample(data_set.begin(), data_set.begin() + load, std::back_inserter(find_vector), test_load, generator);

    std::sample(erase_vector.begin(), erase_vector.end(), std::back_inserter(add_vector), test_load, generator);

    std::sample(data_set.begin() + test_load, data_set.end(), std::back_inserter(unknown_vector), test_load, generator);
}

static void InitTestVector(std::vector<uint64_t>& data_set)
{
    erase_vector.reserve(test_vector_size);
    add_vector.reserve(test_vector_size);
    find_vector.reserve(test_vector_size);
    unknown_vector.reserve(test_vector_size);
}

template <typename TKey, typename TObject>
void Bench(std::vector<uint64_t>& data_set, uint64_t startLoad, uint64_t maxLoad, uint64_t stepLoad, BencObjecthInterface<TKey>& object, uint64_t BenchFlags)
{
    uint64_t reserve = startLoad;

    auto name = object.short_name();

    TestType tt = TestType::UNKNOWN;

    bool bReuse = (BenchFlags & 0x0000'1000'0000'0000);

    if (BenchFlags & 0x0000'0100'0000'0000)
    {
        name = name + "/test2"; tt = TestType::TEST2;
    }
    else if (BenchFlags & 0x0000'0010'0000'0000)
    {
        name = name + "/test3"; tt = TestType::TEST3;
    }
    else if (BenchFlags & 0x0000'0001'0000'0000)
    {
        name = name + "/test4"; tt = TestType::TEST4;
    }
    else if (BenchFlags & 0x0000'0000'1000'0000)
    {
        name = name + "/test5"; tt = TestType::TEST5;
    }
    else
    {
        name = name + "/test1"; tt = TestType::TEST1;

        if (BenchFlags & 0x1000'0000'0000'0000) // AddUnique
        {
            name = name + "/unique";
        }
    }

    assert(tt != TestType::UNKNOWN);

    if (BenchFlags & 0x0000'1000'0000'0000)
    {
        name = name + "/reuse";
    }

    if (BenchFlags & 0x0100'0000'0000'0000) // -rmax
    {
        reserve = maxLoad; name = name + "/rmax";
    }
    else if (BenchFlags & 0x0010'0000'0000'0000) // -rmin
    {
        reserve = 1'000'000; name = name + "/rmin";
    }
    else if (BenchFlags & 0x0001'0000'0000'0000) // -ravg
    {
        reserve = maxLoad / 2;  name = name + "/ravg";
    }

    name = name + "/" + generate_python_data_mode;

    name = name + "/" + std::to_string(reserve);

    name = name + "/" + std::to_string(startLoad);

    name = name + "+" + std::to_string(stepLoad) + "'";

    std::cout << name << std::endl;

    _SetConsoleTitle(name);

    std::stringstream result;
    
    memory_usage_start = GetCurrentMemoryUse();

    for (uint64_t load = startLoad; load <= maxLoad; load += stepLoad)
    {
        object.Init(static_cast<uint32_t>(reserve), bReuse, tt);

        object.Bench(BenchFlags, result, data_set, load, tt);
    }
    
    if (generate_python_data > 0)
    {
        name = name + ", '" + Version(MZ::SimdHash::Build, false) + "'";
        
        std::cerr << "data" << generate_python_data << " = [" << std::endl;
        std::cerr << "[" << name << "]," << std::endl << "[";
        std::cerr << result.str();
        std::cerr << "]]" << std::endl;

        generate_python_data++;
    }
}

template <typename TKey, typename TObject>
class BenchObjectWrapper : public BencObjecthInterface<TKey>
{
public:

    std::unique_ptr<TObject> object;

    std::string short_name()
    {
        std::string name = this->name();

        std::string s = name.substr(6, name.find("<") - 6);

        name = name.substr(s.size() + 6);

        const static std::string ankerl("ankerl::unordered_dense::");

        if (s.find(ankerl) != s.npos)
        {
            if constexpr (isSet<TObject, TKey>)
                s = "ankerl::" + s.substr(ankerl.size(), 8) + "set";
            else
                s = "ankerl::" + s.substr(ankerl.size(), 8) + "map";
        }

        std::string SimdHashMemoryMode = "Fast";

        if (s.find("MZ::SimdHash::") != s.npos)
        {
            switch ((MZ::SimdHash::Mode)std::atoi(name.c_str() + name.size() - 4))
            {
            case MZ::SimdHash::Mode::Fast:
                SimdHashMemoryMode = "Fast";
                break;
            case MZ::SimdHash::Mode::FastDivMod:
                SimdHashMemoryMode = "FastDivMod";
                break;
            case MZ::SimdHash::Mode::SaveMemoryFast:
                SimdHashMemoryMode = "SaveMemoryFast";
                break;
            case MZ::SimdHash::Mode::SaveMemoryOpt:
                SimdHashMemoryMode = "SaveMemoryOpt";
                break;
            case MZ::SimdHash::Mode::SaveMemoryMax:
                SimdHashMemoryMode = "SaveMemoryMax";
                break;
            default:
                SimdHashMemoryMode = "Mode=???";
                break;
            }
        }

        bool bFix = (s.find("MZ::SimdHash::") != s.npos && name[name.size() - 2] == '1');

        if (s.find("MZ::SimdHash::") != s.npos)
        {
            if (bFix)
                s = s + "<" + SimdHashMemoryMode + ", Fix>";
            else
                s = s + "<" + SimdHashMemoryMode + ">";
        }

        s = "'" + s + "', '";

        if (name.find("<uint64_t") != name.npos)
        {
            s.append("uint64");
        }

        if (name.find("<uint32_t") != name.npos)
        {
            s.append("uint32");
        }

        if (name.find("absl::hash_internal::Hash<") != name.npos)
        {
            s.append("/abslhash");
        }

        if (name.find("MZ::SimdHash::Hash<") != name.npos)
        {
            s.append("/simdhash");
        }

        if (name.find(ankerl) != name.npos && name.find("hash<") != name.npos)
        {
            s.append("/ankerlhash");
        }

        if (name.find("std::hash<") != name.npos)
        {
            s.append("/stdhash");
        }

        return s;
    }

    std::string name()
    {
        std::string s(typeid(*object).name());

        s = std::regex_replace(s, std::regex("unsigned __int64"), "uint64_t");
        s = std::regex_replace(s, std::regex("unsigned __int32"), "uint32_t");
        s = std::regex_replace(s, std::regex("unsigned int"), "uint32_t");
        s = std::regex_replace(s, std::regex("unsigned __int16"), "uint16_t");
        s = std::regex_replace(s, std::regex("unsigned __int8"), "uint8_t");

        return s; 
    }

    static constexpr auto max_load_factor = CalcLoadFactor(16);

    virtual void Init(uint32_t reserve, bool bReuse, TestType tt)
    {
        auto size = static_cast<uint32_t>(reserve * 0.9);

        if (bReuse)
        {
            if (object.get() == nullptr)
            {
                object.reset(new TObject);

                object->max_load_factor(max_load_factor);

                if (reserve > 0)
                {
                    if constexpr (isSimd<TObject>)
                        object->Resize(size);
                    else
                        object->reserve(size);
                }
            }
            else
            {
                if (tt == TestType::TEST1)
                {
                    if constexpr (isSimd<TObject>)
                    {
                        object->Clear();
                    }
                    else
                    {
                        object->erase(object->begin(), object->end());
                    }
                }
            }
        }
        else
        {
            if (object.get() != nullptr)
            {
                delete object.release();
            }

            object.reset(new TObject);

            object->max_load_factor(max_load_factor);

            if (reserve > 0)
            {
                if constexpr (isSimd<TObject>)
                    object->Resize(size);
                else
                    object->reserve(size);
            }
        }
    }

    void Bench(uint64_t BenchFlags, std::stringstream& result, std::vector<uint64_t>& data_set, uint64_t load, TestType tt) override
    {
        auto memCheckpoint = GetCurrentMemoryUse();

        const auto capacity = Capacity();

        auto test_load = load / 100;

        MakeTestVector(data_set, load, test_load);

        bool bReuse = (BenchFlags & 0x0000'1000'0000'0000);

        bool bUnique = (BenchFlags & 0x1000'0000'0000'0000);

        memory_usage_start += (GetCurrentMemoryUse() - memCheckpoint); // fix mem usage

        auto t_start = std::chrono::high_resolution_clock::now();

        if (tt == TestType::TEST1 || !bReuse)
        {
            assert(TestAdd(data_set.data(), static_cast<uint32_t>(load), bUnique));
        }
        else
        {
            assert(TestAdd(data_set.data() + Count(), static_cast<uint32_t>(load - Count()), bUnique));
        }

        uint64_t opCounter = Count();

        assert(load == opCounter);

        if (tt == TestType::TEST2)
        {
            opCounter = 0;

            t_start = std::chrono::high_resolution_clock::now();

            if constexpr (!isIndex<TObject, TKey>)
            {
                assert(TestRemove(erase_vector.data(), static_cast<uint32_t>(test_load)));
                opCounter += test_load;

                assert(!TestRemove(erase_vector.data(), static_cast<uint32_t>(test_load)));
                opCounter += test_load;

                assert(!TestContains(erase_vector.data(), static_cast<uint32_t>(test_load)));
                opCounter += test_load;

                assert(!TestContains(add_vector.data(), static_cast<uint32_t>(test_load)));
                opCounter += test_load;

                assert(!TestContains(unknown_vector.data(), static_cast<uint32_t>(test_load)));
                opCounter += test_load;

                assert(TestAdd(add_vector.data(), static_cast<uint32_t>(test_load), bUnique));
                opCounter += test_load;

                assert(!TestAdd(add_vector.data(), static_cast<uint32_t>(test_load), false));
                opCounter += test_load;

                assert(TestContains(find_vector.data(), static_cast<uint32_t>(test_load)));
                opCounter += test_load;

                assert(TestRemove(erase_vector.data(), static_cast<uint32_t>(test_load)));
                opCounter += test_load;

                assert(!TestRemove(erase_vector.data(), static_cast<uint32_t>(test_load)));
                opCounter += test_load;

                assert(TestAdd(add_vector.data(), static_cast<uint32_t>(test_load), bUnique));
                opCounter += test_load;

                assert(!TestAdd(add_vector.data(), static_cast<uint32_t>(test_load), false));
                opCounter += test_load;

            }

            assert(TestContains(erase_vector.data(), static_cast<uint32_t>(test_load)));
            opCounter += test_load;

            assert(TestContains(add_vector.data(), static_cast<uint32_t>(test_load)));
            opCounter += test_load;

            assert(TestContains(find_vector.data(), static_cast<uint32_t>(test_load)));
            opCounter += test_load;

            assert(!TestContains(unknown_vector.data(), static_cast<uint32_t>(test_load)));
            opCounter += test_load;
        }

        if (tt == TestType::TEST3)
        {
            opCounter = 0;

            t_start = std::chrono::high_resolution_clock::now();

            auto size = load * 5 / 100;

            assert(TestContains(data_set.data(), static_cast<uint32_t>(size)));
            opCounter += size;

            assert(TestContains(data_set.data() + load / 2 - size / 2, static_cast<uint32_t>(size)));
            opCounter += size;

            assert(TestContains(data_set.data() + load - size, static_cast<uint32_t>(size)));
            opCounter += size;
        }

        if (tt == TestType::TEST4)
        {
            opCounter = 0;

            t_start = std::chrono::high_resolution_clock::now();

            auto size = load * 5 / 100;

            assert(TestContains(data_set.data(), static_cast<uint32_t>(size), true));
            opCounter += size;

            assert(TestContains(data_set.data() + load / 2 - size / 2, static_cast<uint32_t>(size), true));
            opCounter += size;

            assert(TestContains(data_set.data() + load - size, static_cast<uint32_t>(size), true));
            opCounter += size;
        }

        if (tt == TestType::TEST5)
        {
            opCounter = 0;

            t_start = std::chrono::high_resolution_clock::now();

            uint64_t sum = 0;

            if constexpr (!isSet<TObject, TKey>)
            {
                for (uint32_t i = 0; i < 10; i++)
                {
                    for (const auto& entry : *object)
                    {
                        opCounter++;

                        if constexpr (isSimd<TObject>)
                            sum += entry.value;
                        else
                            sum += entry.second;
                    }
                }
            }
            else
            {
                for (uint32_t i = 0; i < 10; i++)
                {
                    for (const auto& key : *object)
                    {
                        opCounter++;

                        sum += key;
                    }
                }
            }
        }

        auto mem = (GetCurrentMemoryUse() - memory_usage_start);

        std::chrono::nanoseconds time = (std::chrono::high_resolution_clock::now() - t_start);

        uint64_t _KeysSum = 0;
        for (uint64_t i = 0; i < load; i++) _KeysSum += static_cast<TKey>(data_set[i]);

        assert(_KeysSum == KeysSum());

        using namespace std::chrono_literals;

        std::cout << "time:" << std::setw(6) << std::fixed << std::setprecision(3)
            << time / 1.0s << std::setprecision(4) << "s, lf: " << load_factor()
            << ", l:" << std::setw(12) << Count();

        if (tt != TestType::TEST5)
            std::cout << ", op:" << std::setw(4) << time / 1ns / opCounter << "ns";
        else
            std::cout << ", op:" << std::setw(5) << std::fixed << std::setprecision(2) << time / 1.0ns / opCounter << "ns";

        std::cout << "," << std::setw(12) << capacity << "," << std::setw(12) << Capacity() << ", ";
        std::cout << std::setw(5) << std::fixed << std::setprecision(3) << mem / 1024.0 / 1024.0 / 1024.0;

        std::cout << "," << std::setw(3) << mem / load;
        std::cout << "," << std::setw(7) << std::fixed << std::setprecision(3) << mem / static_cast<double>(Capacity());

        std::cout << std::endl;

        if (generate_python_data > 0)
        {
            result << std::setprecision(3) << "{\"time\":" << time / 1.0s;
            result << ",\"cnt\":" << Count();

            if (tt != TestType::TEST5)
                result << ",\"op\":" << time / 1ns / opCounter;
            else
                result << ",\"op\":" << std::fixed << std::setprecision(2) << time / 1.0ns / opCounter;

            result << ",\"mem\":" << mem / 1024 / 1024;
            result << "}," << std::endl;
        }
    }

    void Clear() override
    {
        if constexpr (isSimd<TObject>)
        {
            object->Clear();
        }
        else
        {
            object->clear();
        }
    }

    uint32_t Capacity() const override
    {
        if constexpr (isSimd<TObject>)
            return object->Capacity();
        else
            return static_cast<uint32_t>(object->bucket_count());
    }

    uint32_t Count() const override
    {
        if constexpr (isSimd<TObject>)
            return object->Count();
        else
            return static_cast<uint32_t>(object->size());
    }

    float load_factor() const
    {
        return static_cast<float>(object->load_factor());
    }

    bool TestContains(uint64_t* data_set, uint32_t load, bool bReverse = false) override
    {
        bool bContains = true;

        if (bReverse)
        {
            for (int64_t i = (int64_t)(load - 1); i >= 0; i--)
            {
                if constexpr (isSimd<TObject>)
                    bContains &= object->Contains(static_cast<TKey>(data_set[i]));
                else
                    bContains &= object->contains(static_cast<TKey>(data_set[i]));
            }
        }
        else
        {
            for (uint32_t i = 0; i < load; i++)
            {
                if constexpr (isSimd<TObject>)
                    bContains &= object->Contains(static_cast<TKey>(data_set[i]));
                else
                    bContains &= object->contains(static_cast<TKey>(data_set[i]));
            }
        }

        return bContains;
    }

    bool TestRemove(uint64_t* data_set, uint32_t load) override
    {
        bool bRemove = true;

        for (uint32_t i = 0; i < load; i++)
        {
            if constexpr (isSimd<TObject>)
            {
                if constexpr (!isIndex<TObject, TKey>)
                {
                    bRemove &= object->Remove(static_cast<TKey>(data_set[i]));
                }
            }
            else
            {
                bRemove &= (object->erase(static_cast<TKey>(data_set[i])) == 1);
            }
        }

        return bRemove;
    }

    bool TestAdd(uint64_t* data_set, uint32_t load, bool bUnique) override
    {
        bool bAdd = true;

        if constexpr (isSimd<TObject>)
        {
            if (bUnique)
            {
                for (uint32_t i = 0; i < load; i++)
                {
                    if constexpr (isSet<TObject, TKey>)
                        bAdd &= object->Add<true>(static_cast<TKey>(data_set[i]));
                    else
                        bAdd &= object->Add<true>(static_cast<TKey>(data_set[i]), static_cast<TKey>(i));
                }
            }
            else
            {
                for (uint32_t i = 0; i < load; i++)
                {
                    if constexpr (isSet<TObject, TKey>)
                        bAdd &= object->Add<false>(static_cast<TKey>(data_set[i]));
                    else
                        bAdd &= object->Add<false>(static_cast<TKey>(data_set[i]), static_cast<TKey>(i));
                }
            }
        }
        else
        {
            for (uint32_t i = 0; i < load; i++)
            {
                if constexpr (isSet<TObject, TKey>)
                {
                    bAdd &= object->emplace(static_cast<TKey>(data_set[i])).second;
                }
                else
                {
                    bAdd &= object->emplace(static_cast<TKey>(data_set[i]), static_cast<TKey>(i)).second;
                }
            }
        }

        return bAdd;
    }

    uint64_t KeysSum() override
    {
        uint64_t sum = 0;

        if constexpr (isSet<TObject, TKey>)
        {
            for (const auto& key : *object)
            {
                sum += key;
            }
        }
        else
        {
            for (const auto& entry : *object)
            {
                if constexpr (isSimd<TObject>)
                    sum += entry.key;
                else
                    sum += entry.first;
            }
        }

        return sum;
    }
};

template <typename TKey, MZ::SimdHash::Mode mode = MZ::SimdHash::Mode::Fast, bool bFix>
void BenchSimdHashMap(uint64_t BenchFlags, std::vector<uint64_t>& data_set, uint64_t startLoad, uint64_t maxLoad, uint64_t stepLoad)
{
    using ankerlhash = ankerl::unordered_dense::hash<TKey>;

    if (BenchFlags & 8)
    {
        using TObject = BenchObjectWrapper<TKey, MZ::SimdHash::Map<TKey, TKey, ankerlhash, mode, bFix>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 2)
    {
        using TObject = BenchObjectWrapper<TKey, MZ::SimdHash::Map<TKey, TKey, std::hash<TKey>, mode, bFix>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 4)
    {
        using TObject = BenchObjectWrapper<TKey, MZ::SimdHash::Map<TKey, TKey, absl::Hash<TKey>, mode, bFix>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 1)
    {
        using TObject = BenchObjectWrapper<TKey, MZ::SimdHash::Map<TKey, TKey, MZ::SimdHash::Hash<TKey>, mode, bFix>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
}

template <typename TKey, bool bFix>
void BenchSimdHashSet(uint64_t BenchFlags, std::vector<uint64_t>& data_set, uint64_t startLoad, uint64_t maxLoad, uint64_t stepLoad)
{
    using ankerlhash = ankerl::unordered_dense::hash<TKey>;

    if (BenchFlags & 8)
    {
        using TObject = BenchObjectWrapper<TKey, MZ::SimdHash::Set<TKey, ankerlhash, MZ::SimdHash::Mode::Fast, bFix>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 2)
    {
        using TObject = BenchObjectWrapper<TKey, MZ::SimdHash::Set<TKey, std::hash<TKey>, MZ::SimdHash::Mode::Fast, bFix>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 4)
    {
        using TObject = BenchObjectWrapper<TKey, MZ::SimdHash::Set<TKey, absl::Hash<TKey>, MZ::SimdHash::Mode::Fast, bFix>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 1)
    {
        using TObject = BenchObjectWrapper<TKey, MZ::SimdHash::Set<TKey, MZ::SimdHash::Hash<TKey>, MZ::SimdHash::Mode::Fast, bFix>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
}

template <typename TKey, bool bFix>
void BenchSimdHashIndex(uint64_t BenchFlags, std::vector<uint64_t>& data_set, uint64_t startLoad, uint64_t maxLoad, uint64_t stepLoad)
{
    using ankerlhash = ankerl::unordered_dense::hash<TKey>;

    if (BenchFlags & 8)
    {
        using TObject = BenchObjectWrapper<TKey, MZ::SimdHash::Index<TKey, ankerlhash, MZ::SimdHash::Mode::Fast, bFix>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 2)
    {
        using TObject = BenchObjectWrapper<TKey, MZ::SimdHash::Index<TKey, std::hash<TKey>, MZ::SimdHash::Mode::Fast, bFix>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 4)
    {
        using TObject = BenchObjectWrapper<TKey, MZ::SimdHash::Index<TKey, absl::Hash<TKey>, MZ::SimdHash::Mode::Fast, bFix>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 1)
    {
        using TObject = BenchObjectWrapper<TKey, MZ::SimdHash::Index<TKey, MZ::SimdHash::Hash<TKey>, MZ::SimdHash::Mode::Fast, bFix>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
}

template <typename TKey>
void BenchTslRobinMap(uint64_t BenchFlags, std::vector<uint64_t>& data_set, uint64_t startLoad, uint64_t maxLoad, uint64_t stepLoad)
{
    using ankerlhash = ankerl::unordered_dense::hash<TKey>;

    if (BenchFlags & 8)
    {
        using TObject = BenchObjectWrapper<TKey, tsl::robin_map<TKey, TKey, ankerlhash>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 2)
    {
        using TObject = BenchObjectWrapper<TKey, tsl::robin_map<TKey, TKey, std::hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 4)
    {
        using TObject = BenchObjectWrapper<TKey, tsl::robin_map<TKey, TKey, absl::Hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 1)
    {
        using TObject = BenchObjectWrapper<TKey, tsl::robin_map<TKey, TKey, MZ::SimdHash::Hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
}

template <typename TKey>
void BenchTslRobinSet(uint64_t BenchFlags, std::vector<uint64_t>& data_set, uint64_t startLoad, uint64_t maxLoad, uint64_t stepLoad)
{
    using ankerlhash = ankerl::unordered_dense::hash<TKey>;

    if (BenchFlags & 8)
    {
        using TObject = BenchObjectWrapper<TKey, tsl::robin_set<TKey, ankerlhash>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 2)
    {
        using TObject = BenchObjectWrapper<TKey, tsl::robin_set<TKey, std::hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 4)
    {
        using TObject = BenchObjectWrapper<TKey, tsl::robin_set<TKey, absl::Hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 1)
    {
        using TObject = BenchObjectWrapper<TKey, tsl::robin_set<TKey, MZ::SimdHash::Hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
}

template <typename TKey>
void BenchAbslFlatHashMap(uint64_t BenchFlags, std::vector<uint64_t>& data_set, uint64_t startLoad, uint64_t maxLoad, uint64_t stepLoad)
{
    using ankerlhash = ankerl::unordered_dense::hash<TKey>;

    if (BenchFlags & 8)
    {
        using TObject = BenchObjectWrapper<TKey, absl::flat_hash_map<TKey, TKey, ankerlhash>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 2)
    {
        using TObject = BenchObjectWrapper<TKey, absl::flat_hash_map<TKey, TKey, std::hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 4)
    {
        using TObject = BenchObjectWrapper<TKey, absl::flat_hash_map<TKey, TKey, absl::Hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 1)
    {
        using TObject = BenchObjectWrapper<TKey, absl::flat_hash_map<TKey, TKey, MZ::SimdHash::Hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
}

template <typename TKey>
void BenchAbslFlatHashSet(uint64_t BenchFlags, std::vector<uint64_t>& data_set, uint64_t startLoad, uint64_t maxLoad, uint64_t stepLoad)
{
    using ankerlhash = ankerl::unordered_dense::hash<TKey>;

    if (BenchFlags & 8)
    {
        using TObject = BenchObjectWrapper<TKey, absl::flat_hash_set<TKey, ankerlhash>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 2)
    {
        using TObject = BenchObjectWrapper<TKey, absl::flat_hash_set<TKey, std::hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 4)
    {
        using TObject = BenchObjectWrapper<TKey, absl::flat_hash_set<TKey, absl::Hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 1)
    {
        using TObject = BenchObjectWrapper<TKey, absl::flat_hash_set<TKey, MZ::SimdHash::Hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
}

template <typename TKey>
void BenchEm7HashMap(uint64_t BenchFlags, std::vector<uint64_t>& data_set, uint64_t startLoad, uint64_t maxLoad, uint64_t stepLoad)
{
    using ankerlhash = ankerl::unordered_dense::hash<TKey>;

    if (BenchFlags & 8)
    {
        using TObject = BenchObjectWrapper<TKey, emhash7::Map<TKey, TKey, ankerlhash>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 2)
    {
        using TObject = BenchObjectWrapper<TKey, emhash7::Map<TKey, TKey, std::hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 4)
    {
        using TObject = BenchObjectWrapper<TKey, emhash7::Map<TKey, TKey, absl::Hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 1)
    {
        using TObject = BenchObjectWrapper<TKey, emhash7::Map<TKey, TKey, MZ::SimdHash::Hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
}

template <typename TKey>
void BenchEm8HashMap(uint64_t BenchFlags, std::vector<uint64_t>& data_set, uint64_t startLoad, uint64_t maxLoad, uint64_t stepLoad)
{
    using ankerlhash = ankerl::unordered_dense::hash<TKey>;

    if (BenchFlags & 8)
    {
        using TObject = BenchObjectWrapper<TKey, emhash8::HashMap<TKey, TKey, ankerlhash>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 2)
    {
        using TObject = BenchObjectWrapper<TKey, emhash8::HashMap<TKey, TKey, std::hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 4)
    {
        using TObject = BenchObjectWrapper<TKey, emhash8::HashMap<TKey, TKey, absl::Hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 1)
    {
        using TObject = BenchObjectWrapper<TKey, emhash8::HashMap<TKey, TKey, MZ::SimdHash::Hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
}

template <typename TKey>
void BenchAnkerlHashMap(uint64_t BenchFlags, std::vector<uint64_t>& data_set, uint64_t startLoad, uint64_t maxLoad, uint64_t stepLoad)
{
    using ankerlhash = ankerl::unordered_dense::hash<TKey>;

    if (BenchFlags & 8)
    {
        using TObject = BenchObjectWrapper<TKey, ankerl::unordered_dense::map<TKey, TKey, ankerlhash>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 2)
    {
        using TObject = BenchObjectWrapper<TKey, ankerl::unordered_dense::map<TKey, TKey, std::hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 4)
    {
        using TObject = BenchObjectWrapper<TKey, ankerl::unordered_dense::map<TKey, TKey, absl::Hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
    if (BenchFlags & 1)
    {
        using TObject = BenchObjectWrapper<TKey, ankerl::unordered_dense::map<TKey, TKey, MZ::SimdHash::Hash<TKey>>>;
        TObject object; Bench<TKey, TObject>(data_set, startLoad, maxLoad, stepLoad, object, BenchFlags);
    }
}
#endif
