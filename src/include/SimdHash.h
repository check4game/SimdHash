#ifndef __SIMDHASH_H__
#define __SIMDHASH_H__

#include <intrin.h>

#if defined(NDEBUG)
#undef NDEBUG
#include <assert.h>
#define NDEBUG
#else
#include <assert.h>
#endif

namespace MZ
{
    namespace SimdHash
    {
        static constexpr uint32_t Build = 1023;
       
        static __forceinline const uint32_t ResetLowestSetBit(const uint32_t value)
        {
            return value & (value - 1);
        }

        template <bool bFix>
        static __forceinline const uint32_t TrailingZeroCount(uint32_t mask)
        {
            if constexpr (bFix) // fix for old CPU
            {
                if (mask & 0x0001) return 0;
                if (mask & 0x0002) return 1;
                if (mask & 0x0004) return 2;
                if (mask & 0x0008) return 3;
                if (mask & 0x0010) return 4;
                if (mask & 0x0020) return 5;
                if (mask & 0x0040) return 6;
                if (mask & 0x0080) return 7;

                mask >>= 8;

                if (mask & 0x0001) return 8;
                if (mask & 0x0002) return 9;
                if (mask & 0x0004) return 10;
                if (mask & 0x0008) return 11;
                if (mask & 0x0010) return 12;
                if (mask & 0x0020) return 13;
                if (mask & 0x0040) return 14;
                if (mask & 0x0080) return 15;

                return 16;
            }
            else
            {
                return _tzcnt_u32(mask);
            }
        }

        template <typename TKey>
        struct Hash 
        {
            uint64_t operator()(TKey const& key) const
            {
                static constexpr auto a = UINT64_C(11400714819323198485);

                if constexpr (std::is_same<TKey, uint64_t>::value)
                {
                    return (key ^ (key >> 32)) * a;
                }
                else if constexpr (std::is_same<TKey, uint32_t>::value)
                {
                    return (key ^ (key >> 16)) * a;
                }
                else if constexpr (std::is_same<TKey, int64_t>::value)
                {
                    auto key0 = static_cast<uint64_t>(key);
                    return (key0 ^ (key0 >> 32)) * a;
                }
                else if constexpr (std::is_same<TKey, int32_t>::value)
                {
                    auto key0 = static_cast<uint32_t>(key);
                    return (key0 ^ (key0 >> 16)) * a;
                }
                else
                {
                    return std::hash<TKey>()(key);
                }
            }
        };
      
        static __forceinline uint32_t RoundUpToPowerOf2(uint32_t value)
        {
            return static_cast<uint32_t>(UINT64_C(0x1'0000'0000) >> __lzcnt(value - 1));
        }

        class TagVector
        {
            __m128i xmm;

        public:

            TagVector() = default;

            explicit TagVector(uint8_t v)
            {
                xmm = _mm_set1_epi8(static_cast<int8_t>(v));
            }

            explicit TagVector(int v)
            {
                xmm = _mm_set1_epi8(static_cast<int8_t>(v));
            }

            operator __m128i() const { return xmm; }

            __forceinline void Load(const uint8_t* ptr)
            {
                xmm = _mm_loadu_si128((const __m128i*)ptr);
            }
            
            template <bool bAlign, bool bStream = true>
            __forceinline void Store(uint8_t* ptr) const
            {
                if constexpr (bAlign && bStream)
                    _mm_stream_si128((__m128i*)ptr, xmm);
                else if constexpr (bAlign)
                    _mm_store_si128((__m128i*)ptr, xmm);
                else
                    _mm_storeu_si128((__m128i*)ptr, xmm);
            }

            __forceinline uint32_t GetEmptyMask() const
            {
                return static_cast<uint32_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(EMPTY_VECTOR, *this)));
            }

            __forceinline uint32_t GetEmptyOrTomeStoneMask() const
            {
                return static_cast<uint32_t>
                    (_mm_movemask_epi8(
                        _mm_cmpeq_epi8(EMPTY_VECTOR,
                            _mm_and_si128(FORBIDDEN_VECTOR, *this))));
            }

            __forceinline uint32_t GetCmpMask(const TagVector& vector) const
            {
                return static_cast<uint32_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(vector, *this)));
            }

            static const TagVector EMPTY_VECTOR, FORBIDDEN_VECTOR;

            static constexpr uint8_t SIZE = 16, EMPTY = 0x80, TOMBSTONE = 0x81, FORBIDDEN = 0x82;
        };

        const TagVector TagVector::EMPTY_VECTOR = TagVector(TagVector::EMPTY);

        const TagVector TagVector::FORBIDDEN_VECTOR = TagVector(TagVector::FORBIDDEN);

#pragma pack(push, 1)

        enum class Type { Map, Set, Index };

        template<typename TKey, typename TValue, Type T>
        struct Entry;

        template<typename TKey, typename TValue>
        struct Entry<TKey, TValue, Type::Map> {
            TKey key;
            TValue value;
        };

        template<typename TKey, typename TValue>
        struct Entry<TKey, TValue, Type::Set> {
            TKey key;
        };

        template<typename TKey, typename TValue>
        struct Entry<TKey, TValue, Type::Index> {
            TKey key;
            uint32_t realIndex;
        };

#pragma pack(pop)

        class TagsArray
        {
        public:

            TagsArray() : _ptr(nullptr), _size(0) {}

            TagsArray(TagsArray&& other) noexcept : _ptr(other._ptr), _size(other._size)
            {
                other._ptr = nullptr;
                other._size = 0;
            }

            ~TagsArray()
            {
                Clear();
            }

            void Clear()
            {
                if (_ptr)
                {
                    delete[] _ptr; _ptr = nullptr;
                }
                _size = 0;
            }
            
            __forceinline uint8_t& operator[](uint64_t index)
            {
                return _ptr[index];
            }

            __forceinline const uint8_t& operator[](uint64_t index) const
            {
                return _ptr[index];
            }

            __forceinline uint8_t* begin() const { return _ptr; }

            __forceinline uint8_t* end() const { return _ptr + _size; }

            __forceinline const uint8_t* data() const { return _ptr; }

            template<bool bUseStd = false>
            void Init()
            {
                if constexpr (bUseStd)
                {
                    std::fill_n(begin(), size() - TagVector::SIZE, TagVector::EMPTY);
                    std::fill_n(end() - TagVector::SIZE, TagVector::SIZE, TagVector::FORBIDDEN);
                }
                else
                {
                    assert(0 == (size() % TagVector::SIZE));

                    auto padding = (TagVector::SIZE - (reinterpret_cast<uintptr_t>(begin()) & (TagVector::SIZE - 1))) & (TagVector::SIZE - 1);

                    if (0 != padding)
                    {
                        TagVector::EMPTY_VECTOR.Store<false>(begin());
                    }

                    for (uint8_t* ptr = begin() + padding; ptr < (end() - TagVector::SIZE); ptr += TagVector::SIZE)
                    {
                        TagVector::EMPTY_VECTOR.Store<true>(ptr);
                    }

                    TagVector::FORBIDDEN_VECTOR.Store<false>(end() - TagVector::SIZE);
                }
            }

            uint32_t size() { return _size; }

            void AdjustSize(uint32_t size)
            {
                assert(size > _size);

                if (_ptr) Clear();

                _ptr = new uint8_t[size];

                _size = size;
            }

        protected:

            uint32_t _size = 0;

            uint8_t* _ptr = nullptr;
        };

        constexpr int CalcShift(uint32_t x)
        {
            if (x == 0) return 32;
            int count = 0;

            while ((x & 1) == 0)
            {
                x >>= 1;
                count++;
            }

            return count;
        }

        template<typename TEntry, uint32_t TPageSize = 4096>
        class SmartArray
        {
            static constexpr uint32_t shift = CalcShift(TPageSize);

            static constexpr uint32_t mask = TPageSize - 1;

            TEntry** _pages = nullptr;

            uint32_t _capacity = 0;

            static_assert((TPageSize& (TPageSize - 1)) == 0, "TPageSize must be a power of two");

        public:

            static constexpr uint32_t PageSize = (TPageSize != 0) ? TPageSize : 4096;

            SmartArray() = default;

            ~SmartArray()
            {
                if (_pages)
                {
                    if constexpr (TPageSize >= 1024)
                    {
                        for (uint32_t i = 0; i < _capacity; i++)
                        {
                            delete[] _pages[i];
                        }

                        delete[] _pages; _pages = nullptr;
                    }
                    else
                    {
                        delete[] _pages[0]; delete[] _pages; _pages = nullptr;
                    }
                }
            }

            __forceinline TEntry& operator[](uint64_t index)
            {
                if constexpr (TPageSize >= 1024)
                    return _pages[index >> shift][index & mask];
                else
                    return *_pages[index];
            }

            __forceinline const TEntry& operator[](uint64_t index) const
            {
                if constexpr (TPageSize >= 1024)
                    return _pages[index >> shift][index & mask];
                else
                    return *_pages[index];
            }

            void AdjustSize(uint32_t size)
            {
                if constexpr (TPageSize >= 1024)
                {
                    if ((size % TPageSize) != 0)
                    {
                        size += (TPageSize - (size % TPageSize));
                    }

                    assert(size > _capacity * TPageSize);

                    auto old_pages = _pages;
                    auto old_capacity = _capacity;

                    _pages = new TEntry * [_capacity = size / TPageSize];

                    for (uint32_t i = 0; i < _capacity; i++)
                    {
                        if (i < old_capacity) _pages[i] = old_pages[i]; else _pages[i] = new TEntry[TPageSize];
                    }

                    if (old_pages) delete[] old_pages;
                }
                else
                {
                    assert(size > _capacity);

                    if (!_pages)
                    {
                        _pages = new TEntry[1];
                        _pages[0] = new TEntry[_capacity = size];
                    }
                    else
                    {
                        const auto old_page = _pages[0];

                        _pages[0] = new TEntry[size];

                        std::copy_n(old_page, _capacity, _pages[0]);

                        _capacity = size; delete[] old_page;
                    }
                }
            }
        };

        enum class Mode { Fast = 0, FastDivMod = 1, SaveMemoryFast = 2, SaveMemoryOpt = 4, SaveMemoryMax = 8 };

        template <typename TKey, typename TValue, Type type, class Hash, Mode mode = Mode::Fast, bool bFix = false>
        class Core
        {
        protected:
            TagsArray _tags;

            SmartArray<Entry<TKey, TValue, type>> _entries;

            const Hash _hasher;

        public:
            static constexpr uint32_t MIN_SIZE = 2048;
            static constexpr uint32_t MAX_SIZE = 0x80000000; // 0x80000000 2'147'483'648

            void Clear(uint32_t size = 0)
            {
                _Count = 0;

                if (size > 0 && (AdjustCapacity(size)) != _Capacity)
                {
                    _tags.Clear();

                    _Capacity = 0; Resize(size);
                }
                else
                {
                    _tags.Init();
                }

#if defined(SIMDHASH_TEST)
                PROBE_COUNTER = CMP_COUNTER = 0;
#endif
            }

            uint32_t Count() const
            {
                return _Count; 
            }

            uint32_t Capacity() const
            {
                return _Capacity; 
            }

            float load_factor() const
            {
                return (_Count) ? static_cast<float>((static_cast<double>(_Count) / _Capacity)) : 0.0f;
            }

            float max_load_factor() { return _max_load_factor;  }

            void max_load_factor(float mlf)
            {
                if (mlf >= MIN_LOAD_FACTOR && mlf <= MAX_LOAD_FACTOR)
                {
                    _max_load_factor = mlf;
                }

                _CountGrowthLimit = MAX_SIZE;

                if (_Capacity < MAX_SIZE)
                {
                    _CountGrowthLimit = static_cast<uint32_t>(static_cast<double>(_Capacity) * _max_load_factor);
                }
            }

            void Resize(uint32_t size)
            {
                if (_Capacity > AdjustCapacity(size)) return;

                InitCapacity(size);

                size = _Capacity + TagVector::SIZE;

                if (size == _tags.size()) return;

                _entries.AdjustSize(_Capacity);

                if (_Count == 0)
                {
                    _tags.AdjustSize(size); _tags.Init();
                }
                else
                {
                    if constexpr (type == Type::Index)
                    {
                        _tags.AdjustSize(size); _tags.Init();
                        
                        for (uint32_t realIndex = 0; realIndex < _Count; realIndex++)
                        {
                            auto tupleIndex = _hasher(_entries[realIndex].key);

                            const auto tag = HashToTag(tupleIndex);

                            tupleIndex = FindEmpty(tupleIndex);

                            _tags[tupleIndex] = tag;

                            _entries[tupleIndex].realIndex = realIndex;
                        }
                    }
                    else
                    {
                        TagsArray prevTags(std::move(_tags));

                        _tags.AdjustSize(size); _tags.Init();

                        auto prevCount = _Count; _Count = 0;

                        for (uint32_t i = 0; i < prevTags.size(); i++)
                        {
                            if (prevTags[i] & TagVector::EMPTY) continue;

                            auto prevEntry = _entries[i];

                            auto prevTag = prevTags[i];

                            prevTags[i] = TagVector::EMPTY;

                            while (true)
                            {
                                auto emptyIndex = FindEmpty(_hasher(prevEntry.key));

                                if (emptyIndex >= prevTags.size() || prevTags[emptyIndex] & TagVector::EMPTY)
                                {
                                    _tags[emptyIndex] = prevTag;

                                    _entries[emptyIndex] = prevEntry;

                                    _Count++; break;
                                }

                                const auto saveTag = prevTags[emptyIndex];

                                prevTags[emptyIndex] = TagVector::EMPTY;

                                _tags[emptyIndex] = prevTag; prevTag = saveTag;

                                const auto saveEntry = _entries[emptyIndex];

                                _entries[emptyIndex] = prevEntry; prevEntry = saveEntry;

                                _Count++;
                            }
                        }

                        assert(prevCount == _Count);
                    }
                }
            }

        private:

            uint32_t AdjustCapacity(uint32_t size)
            {
                if (size <= MIN_SIZE) return MIN_SIZE;
                if (size >= MAX_SIZE) return MAX_SIZE;

                if constexpr (mode > Mode::FastDivMod)
                {
                    uint32_t ph = RoundUpToPowerOf2(size);

                    if (ph <= (16 * 1024 * 1024)) return ph;

                    uint32_t pi = static_cast<uint32_t>(mode); // 2,4,8 = SaveMemoryFast, SaveMemoryOpt, SaveMemoryMax
                    
                    if constexpr (mode == Mode::SaveMemoryMax)
                    {
                        if (ph <= (64 * 1024 * 1024))
                            pi = static_cast<uint32_t>(Mode::SaveMemoryOpt);
                        else
                            pi = pi * (ph / 1024 / 1024) / 128; // 8, 16, 32, 64
                    }

                    for (uint32_t i = 1; i < pi; i++)
                    {
                        auto new_size = (static_cast<uint64_t>(ph / 2) * i / pi + ph / 2);
                        
                        new_size = new_size / _entries.PageSize * _entries.PageSize;

                        if (size <= new_size) return static_cast<uint32_t>(new_size);
                    }
                }

                return RoundUpToPowerOf2(size);
            }

            __forceinline uint64_t AdjustHash(const uint64_t hash) const
            {
                if constexpr (mode == Mode::Fast)
                {
                    return hash & _CapacityMask;
                }
                else
                {
                    const uint64_t lowbits = _CapacityMultiplier * static_cast<uint32_t>(hash);
                    return __umulh(lowbits, _Capacity);
                }
            }

            __forceinline uint64_t AdjustTupleIndex(const uint64_t tupleIndex) const
            {
                if constexpr (mode == Mode::Fast)
                {
                    return tupleIndex & _CapacityMask;
                }
                else
                {
                    const uint64_t lowbits = _CapacityMultiplier * tupleIndex;
                    return __umulh(lowbits, _Capacity);
                }
            }

            static constexpr auto MAX_LOAD_FACTOR = static_cast<float>(0.99);
            static constexpr auto DEF_LOAD_FACTOR = static_cast<float>(0.9766);
            static constexpr auto MIN_LOAD_FACTOR = static_cast<float>(0.75);

            float _max_load_factor = DEF_LOAD_FACTOR;

        private:

            void InitCapacity(uint32_t size)
            {
                _Capacity = AdjustCapacity(size);

                if constexpr (mode == Mode::Fast)
                {
                    _CapacityMask = _Capacity - 1;
                }
                else
                {
                    _CapacityMultiplier = UINT64_C(0xFFFFFFFFFFFFFFFF) / _Capacity + 1;
                }

                max_load_factor(_max_load_factor);
            }

#if defined(SIMDHASH_TEST)
        protected:
            uint64_t PROBE_COUNTER = 0, CMP_COUNTER = 0;

        public:

            uint64_t ProbeCounter() { return PROBE_COUNTER; }

            uint64_t CmpCounter() { return CMP_COUNTER; }

            uint64_t KeysSum()
            {
                uint64_t sum = 0;

                for (uint32_t i = 0; i < _tags.size(); i++)
                {
                    if (_tags[i] < TagVector::EMPTY)
                    {
                        if constexpr (type == Type::Index)
                        {
                            sum += _entries[_entries[i].realIndex].key;
                        }
                        else
                        {
                            sum += _entries[i].key;
                        }
                    }
                }

                return sum;
            }
#endif
        protected:

            /// <summary>
            /// Retrieves the 7 most significant bits from the hash.
            /// </summary>
            /// <param name="hash">HashCode</param>
            /// <returns>The 7 most significant bits of the hash, [0...127]</returns>
            __forceinline uint8_t HashToTag(const uint64_t hash) const
            {
                return static_cast<uint8_t>(hash >> 57);
            }

            template<bool bValue, typename TFunc>
            __forceinline bool FindEntry(const TKey& key, TFunc FUNCTION) const
            {
                auto tupleIndex = _hasher(key);

                auto jump = static_cast<uint8_t>(0);

                const TagVector target(HashToTag(tupleIndex));

                TagVector source;

                tupleIndex = AdjustHash(tupleIndex);

                while (true)
                {
                    source.Load(_tags.data() + tupleIndex);

                    auto resultMask = source.GetCmpMask(target);

                    while (resultMask)
                    {
                        if constexpr (type == Type::Index)
                        {
                            const auto realIndex = _entries[tupleIndex + TrailingZeroCount<bFix>(resultMask)].realIndex;

                            if (key == _entries[realIndex].key)
                            {
                                FUNCTION(realIndex); return true;
                            }
                        }

                        if constexpr (type == Type::Set)
                        {
                            const auto realIndex = tupleIndex + TrailingZeroCount<bFix>(resultMask);

                            if (key == _entries[realIndex].key)
                            {
                                FUNCTION(realIndex); return true;
                            }
                        }

                        if constexpr (type == Type::Map)
                        {
                            const auto realIndex = tupleIndex + TrailingZeroCount<bFix>(resultMask);

                            const auto& entry = _entries[realIndex];

                            if (key == entry.key)
                            {
                                if constexpr (bValue)
                                    FUNCTION(entry.value);
                                else
                                    FUNCTION(realIndex);

                                return true;
                            }
                        }

                        resultMask = ResetLowestSetBit(resultMask);
                    }

                    if (source.GetEmptyMask()) return false;

                    tupleIndex = AdjustTupleIndex(tupleIndex + (jump += TagVector::SIZE));
                }

                return false;
            }

            template<bool bUnique, bool bUpdate, typename TFunc>
            __forceinline bool Add(const TKey& key, TFunc FUNCTION)
            {
                auto tupleIndex = _hasher(key);

                const auto tag = HashToTag(tupleIndex);

                auto jump = static_cast<uint8_t>(0);

                uint32_t emptyMask;

                TagVector source;

                tupleIndex = AdjustHash(tupleIndex);

                if constexpr (!bUnique)
                {
                    const TagVector target(tag);

                    while (true)
                    {
                        source.Load(_tags.data() + tupleIndex);

                        auto resultMask = source.GetCmpMask(target);

                        while (resultMask)
                        {
#if defined(SIMDHASH_TEST)
                            CMP_COUNTER++;
#endif
                            const auto entryIndex = tupleIndex + TrailingZeroCount<bFix>(resultMask);

                            if constexpr (type == Type::Index)
                            {
                                const auto realIndex = _entries[entryIndex].realIndex;

                                if (key == _entries[realIndex].key)
                                {
                                    if constexpr (bUpdate) FUNCTION(realIndex);
                                    
                                    return false;
                                }
                            }

                            if constexpr (type == Type::Set)
                            {
                                if (key == _entries[entryIndex].key) return false;
                            }

                            if constexpr (type == Type::Map)
                            {
                                auto& entry = _entries[entryIndex];

                                if (key == entry.key)
                                {
                                    if constexpr (bUpdate) FUNCTION(entry.value);

                                    return false;
                                }
                            }

                            resultMask = ResetLowestSetBit(resultMask);
                        }

                        if (emptyMask = source.GetEmptyOrTomeStoneMask()) break;

#if defined(SIMDHASH_TEST)
                        PROBE_COUNTER++;
#endif
                        tupleIndex = AdjustTupleIndex(tupleIndex + (jump += TagVector::SIZE));
                    }
                }
                else
                {
                    while (true)
                    {
                        source.Load(_tags.data() + tupleIndex);

                        if (emptyMask = source.GetEmptyOrTomeStoneMask()) break;

#if defined(SIMDHASH_TEST)
                        PROBE_COUNTER++;
#endif
                        tupleIndex = AdjustTupleIndex(tupleIndex + (jump += TagVector::SIZE));
                    }
                }

                const auto entryIndex = tupleIndex + TrailingZeroCount<bFix>(emptyMask);

                _tags[entryIndex] = tag;

                if constexpr (type == Type::Index)
                {
                    const auto realIndex = _Count;

                    _entries[entryIndex].realIndex = realIndex;

                    _entries[realIndex].key = key;

                    if constexpr (bUpdate) FUNCTION(realIndex);
                }

                if constexpr (type == Type::Set)
                {
                    _entries[entryIndex].key = key;
                }

                if constexpr (type == Type::Map)
                {
                    auto& entry = _entries[entryIndex];
                    entry.key = key; FUNCTION(entry.value);
                }

                if (++_Count == _CountGrowthLimit) Resize(_Capacity + 1);

                return true;
            }

            __forceinline uint32_t FindEmpty(uint64_t tupleIndex) const
            {
                auto jump = static_cast<uint8_t>(0);

                TagVector source;

                tupleIndex = AdjustHash(tupleIndex);

                while(true)
                {
                    source.Load(_tags.data() + tupleIndex);

                    const auto emptyMask = source.GetEmptyMask();

                    if (emptyMask)
                    {
                        return static_cast<uint32_t>(tupleIndex + TrailingZeroCount<bFix>(emptyMask));
                    }

                    tupleIndex = AdjustTupleIndex(tupleIndex + (jump += TagVector::SIZE));
                }

                return _Capacity;
            }

            __forceinline bool Remove(const TKey& key)
            {
                static_assert(type != Type::Index);

                return FindEntry<false>(key, [this](const auto& entryIndex)
                {
                    _tags[entryIndex] = TagVector::TOMBSTONE; _Count--;
                });
            }

            Core() { Resize(0); }

            uint32_t _Capacity, _CapacityMask;
            
            uint32_t _Count, _CountGrowthLimit;

            uint64_t _CapacityMultiplier;            
        };

        template <typename TKey, typename TValue, class THash = Hash<TKey>, Mode mode = Mode::Fast, bool bFix = false>
        class Map : public Core<TKey, TValue, Type::Map, THash, mode, bFix>
        {
            using core = Core<TKey, TValue, Type::Map, THash, mode, bFix>;

        public:
            Map() : core() {}

            template<bool bUnique = false>
            __forceinline bool Add(const TKey& key, const TValue& value)
            {
                return core::Add<bUnique, false>(key, [&value](auto& _value) { _value = value; });
            }

            __forceinline bool AddOrUpdate(const TKey& key, const TValue& value)
            {
                return core::Add<false, true>(key, [&value](auto& _value) { _value = value; });
            }

            __forceinline bool Update(const TKey& key, const TValue& value)
            {
                return core::FindEntry<true>(key, [&value](auto& _value) { _value = value; });
            }

            __forceinline bool TryGetValue(const TKey& key, TValue& value) const
            {
                return core::FindEntry<true>(key, [&value](const auto& _value) { value = _value; });
            }

            __forceinline bool Contains(const TKey& key) const
            {
                return core::FindEntry<false>(key, [](const auto&) {});
            }

            using core::Remove;
        };

        template <typename TKey, class THash = Hash<TKey>, Mode mode = Mode::Fast, bool bFix = false>
        class Set : public Core<TKey, void, Type::Set, THash, mode, bFix>
        {
            using core = Core<TKey, void, Type::Set, THash, mode, bFix>;

        public:
            Set() : core() {}

            template<bool bUnique = false>
            __forceinline bool Add(const TKey& key)
            {
                return core::Add<bUnique, false>(key, []() {});
            }

            __forceinline bool Contains(const TKey& key) const
            {
                return core::FindEntry<false>(key, [](const auto&) {});
            }

            using core::Remove;
        };

        template <typename TKey, class THash = Hash<TKey>, Mode mode = Mode::Fast, bool bFix = false>
        class Index : public Core<TKey, void, Type::Index, THash, mode, bFix>
        {
            using core = Core<TKey, void, Type::Index, THash, mode, bFix>;

        public:
            Index() : core() {}

            template<bool bUnique = false>
            __forceinline bool Add(const TKey& key)
            {
                return core::Add<bUnique, false>(key, [](const auto&) {});
            }

            __forceinline bool TryAdd(const TKey& key, uint32_t& index)
            {
                return core::Add<false, true>(key, [&index](const auto& _index) { index = _index; });
            }

            __forceinline bool TryGetIndex(const TKey& key, uint32_t& index) const
            {
                return core::FindEntry<false>(key, [&index](const auto& _index) { index = _index; });
            }

            __forceinline uint32_t GetIndex(const TKey& key) const
            {
                uint32_t index = core::Capacity();

                core::FindEntry(key, [&index](const auto& _index) { index = _index; });

                return index;
            }

            __forceinline bool Contains(const TKey& key) const
            {
                return core::FindEntry<false>(key, [](const auto&) {});
            }
        };
    }
}

#endif