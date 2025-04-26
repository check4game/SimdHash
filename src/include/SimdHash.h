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

#include <malloc.h>

namespace MZ
{
    namespace SimdHash
    {
        static constexpr uint32_t Build = 1024;
       
        static __forceinline const uint64_t ResetLowestSetBit(const uint64_t value)
        {
            return value & (value - 1);
        }

        template <bool bFix>
        static __forceinline const uint32_t TrailingZeroCount(uint64_t mask)
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
            }

            return static_cast<uint32_t>(_tzcnt_u64(mask));
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

        template <uint8_t TagVectorSize>
        struct TagVectorType;

        template <>
        struct TagVectorType<16>
        {
            using Type = __m128i;
            using MaskType = uint32_t;
        };

        template <>
        struct TagVectorType<32>
        {
            using Type = __m256i;
            using MaskType = uint32_t;
        };

        template <>
        struct TagVectorType<64> {
            using Type = __m512i;
            using MaskType = uint64_t;
        };

        template<uint8_t TagVectorSize>
        class TagVector
        {
            using MaskType = typename TagVectorType<TagVectorSize>::MaskType;

            static_assert(TagVectorSize == 16 || TagVectorSize == 32 || TagVectorSize == 64,
                "Only values of 16, 32 or 64 are allowed.");

            using TagVectorType = typename TagVectorType<TagVectorSize>::Type;

            TagVectorType xmm;

        public:

            TagVector() = default;

            explicit TagVector(uint8_t v)
            {
                if constexpr (TagVectorSize == 16)
                    xmm = _mm_set1_epi8(static_cast<int8_t>(v));
                else if constexpr (TagVectorSize == 32)
                    xmm = _mm256_set1_epi8(static_cast<int8_t>(v));
                else
                    xmm = _mm512_set1_epi8(static_cast<int8_t>(v));
            }

            operator TagVectorType() const { return xmm; }

            template <bool bAlign = false>
            __forceinline static TagVectorType LoadVector(const uint8_t* ptr)
            {
                if constexpr (TagVectorSize == 16)
                {
                    if constexpr (bAlign)
                        return _mm_load_si128((const __m128i*)ptr);
                    else
                        return _mm_loadu_si128((const __m128i*)ptr);
                }
                else if constexpr (TagVectorSize == 32)
                {
                    if constexpr (bAlign)
                        return _mm256_load_si256((const __m256i*)ptr);
                    else
                        return _mm256_loadu_si256((const __m256i*)ptr);
                }
                else
                {
                    if constexpr (bAlign)
                        return _mm512_load_si512((const __m512i*)ptr);
                    else
                        return _mm512_loadu_si512((const __m512i*)ptr);
                }
            }

            template <bool bAlign = false>
            __forceinline void Load(const uint8_t* ptr)
            {
                xmm = LoadVector<bAlign>(ptr);
            }

            template <bool bAlign, bool bStream = true>
            __forceinline void Store(uint8_t* ptr) const
            {
                if constexpr (bAlign && bStream)
                {
                    if constexpr (TagVectorSize == 16)
                        _mm_stream_si128((__m128i*)ptr, xmm);
                    else if constexpr (TagVectorSize == 32)
                        _mm256_stream_si256((__m256i*)ptr, xmm);
                    else
                        _mm512_stream_si512((__m256i*)ptr, xmm);
                }
                else if constexpr (bAlign)
                {
                    if constexpr (TagVectorSize == 16)
                        _mm_store_si128((__m128i*)ptr, xmm);
                    else if constexpr (TagVectorSize == 32)
                        _mm256_store_si256((__m256i*)ptr, xmm);
                    else
                        _mm512_store_si512((__m512i*)ptr, xmm);
                }
                else
                {
                    if constexpr (TagVectorSize == 16)
                        _mm_storeu_si128((__m128i*)ptr, xmm);
                    else if constexpr (TagVectorSize == 32)
                        _mm256_storeu_si256((__m256i*)ptr, xmm);
                    else
                        _mm512_storeu_si512((__m512i*)ptr, xmm);
                }
            }

            __forceinline uint64_t GetEmptyMask() const
            {
                if constexpr (TagVectorSize == 16)
                    return static_cast<MaskType>(_mm_movemask_epi8(_mm_cmpeq_epi8(EMPTY_VECTOR, xmm)));
                else if constexpr (TagVectorSize == 32)
                    return static_cast<MaskType>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(EMPTY_VECTOR, xmm)));
                else
                    return static_cast<MaskType>(_mm512_cmp_epi8_mask(EMPTY_VECTOR, xmm, _MM_CMPINT_EQ));
            }

            __forceinline uint64_t GetEmptyOrTomeStoneMask() const
            {
                if constexpr (TagVectorSize == 16)
                    return static_cast<MaskType>(_mm_movemask_epi8
                        (_mm_cmpeq_epi8(EMPTY_VECTOR,_mm_and_si128(FORBIDDEN_VECTOR, xmm))));
                else if constexpr (TagVectorSize == 32)
                    return static_cast<MaskType>(_mm256_movemask_epi8
                        (_mm256_cmpeq_epi8(EMPTY_VECTOR, _mm256_and_si256(FORBIDDEN_VECTOR, xmm))));
                else
                    return static_cast<MaskType>(_mm512_cmp_epi8_mask(EMPTY_VECTOR,
                        _mm512_and_si512(FORBIDDEN_VECTOR, xmm), _MM_CMPINT_EQ));
            }

            template <bool bAlign>
            __forceinline static uint64_t GetNonEmptyMask(const uint8_t* ptr)
            {
                if constexpr (TagVectorSize == 16)
                {
                    return static_cast<MaskType>
                        (_mm_movemask_epi8(_mm_cmpeq_epi8(ZERO_VECTOR,
                            _mm_and_si128(EMPTY_VECTOR, LoadVector<bAlign>(ptr)))));
                }
                else if constexpr (TagVectorSize == 32)
                {
                    return static_cast<MaskType>
                        (_mm256_movemask_epi8(_mm256_cmpeq_epi8(ZERO_VECTOR,
                            _mm256_and_si256(EMPTY_VECTOR, LoadVector<bAlign>(ptr)))));
                }
                else
                {
                    return static_cast<MaskType>
                        (_mm512_cmp_epi8_mask(ZERO_VECTOR,
                            _mm512_and_si512(EMPTY_VECTOR, LoadVector<bAlign>(ptr)), _MM_CMPINT_EQ));
                }
            }

            __forceinline uint64_t GetCmpMask(const TagVector& vector) const
            {
                if constexpr (TagVectorSize == 16)
                    return static_cast<MaskType>(_mm_movemask_epi8(_mm_cmpeq_epi8(vector, xmm)));
                else if constexpr (TagVectorSize == 32)
                    return static_cast<MaskType>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(vector, xmm)));
                else
                    return static_cast<MaskType>(_mm512_cmp_epi8_mask(vector, xmm, _MM_CMPINT_EQ));
            }

            static const TagVector EMPTY_VECTOR, FORBIDDEN_VECTOR, ZERO_VECTOR;

            static constexpr uint8_t SIZE = sizeof(xmm), EMPTY = 0x80, TOMBSTONE = 0x81, FORBIDDEN = 0x82, ZERO = 0;
        };

        using TagVector16 = TagVector<16>;

        const TagVector16 TagVector16::EMPTY_VECTOR = TagVector16(TagVector16::EMPTY);
        const TagVector16 TagVector16::FORBIDDEN_VECTOR = TagVector16(TagVector16::FORBIDDEN);
        const TagVector16 TagVector16::ZERO_VECTOR = TagVector16(TagVector16::ZERO);

        using TagVector32 = TagVector<32>;

        const TagVector32 TagVector32::EMPTY_VECTOR = TagVector32(TagVector32::EMPTY);
        const TagVector32 TagVector32::FORBIDDEN_VECTOR = TagVector32(TagVector32::FORBIDDEN);
        const TagVector32 TagVector32::ZERO_VECTOR = TagVector32(TagVector32::ZERO);

#ifdef USE_AVX512
        using TagVector64 = TagVector<64>;

        const TagVector64 TagVector64::EMPTY_VECTOR = TagVector64(TagVector64::EMPTY);
        const TagVector64 TagVector64::FORBIDDEN_VECTOR = TagVector64(TagVector64::FORBIDDEN);
        const TagVector64 TagVector64::ZERO_VECTOR = TagVector64(TagVector64::ZERO);
#endif

        using TagVectorCore = TagVector16;
        using TagVectorIterator = TagVector32;

#pragma pack(push, 1)

        enum class Type { Map, Set, Index };

        template<typename TKey, typename TValue, bool bMap>
        struct Entry;

        template<typename TKey, typename TValue>
        struct Entry<TKey, TValue, true>
        {
            TKey key;
            TValue value;
        };

        template<typename TKey, typename TValue>
        struct Entry<TKey, TValue, false>
        {
            TKey key;
        };

#pragma pack(pop)

        template<typename TagVector>
        class TagArray
        {
        public:

            TagArray() : _ptr(nullptr), _size(0) {}

            TagArray(TagArray&& other) noexcept : _ptr(other._ptr), _size(other._size)
            {
                other._ptr = nullptr;
                other._size = 0;
            }

            ~TagArray()
            {
                Clear();
            }

            void Clear()
            {
                if (_ptr)
                {
                    _aligned_free(_ptr); _ptr = nullptr;
                }
                _size = 0;
            }
            
            __forceinline uint8_t& operator[](uint64_t index)
            {
                return _ptr[index];
            }

            __forceinline const uint8_t operator[](uint64_t index) const
            {
                return _ptr[index];
            }

            __forceinline uint8_t* begin() const
            { 
                return _ptr;
            }

            __forceinline uint8_t* end() const
            {
                return _ptr + _size; 
            }

            __forceinline const uint8_t* data() const
            {
                return _ptr;
            }

            template<bool bUseStd = false>
            void Init()
            {
                if constexpr (bUseStd)
                {
                    std::fill_n(begin(), _size, TagVector::EMPTY);
                    std::fill_n(end(), TagVector::SIZE, TagVector::FORBIDDEN);
                }
                else
                {
                    assert(0 == (_size % TagVector::SIZE));

                    for (uint8_t* ptr = begin(); ptr < end(); ptr += TagVector::SIZE)
                    {
                        TagVector::EMPTY_VECTOR.Store<true, true>(ptr);
                    }

                    TagVector::FORBIDDEN_VECTOR.Store<true, true>(end());
                }
            }

            uint32_t size() const { return _size; }

            void AdjustSize(uint32_t size)
            {
                assert(size > _size);

                if (_ptr) Clear();

                assert(nullptr != (_ptr = static_cast<uint8_t*>(_aligned_malloc(size + TagVector::SIZE, TagVector::SIZE))));

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
        class EntryArray
        {
            static constexpr uint32_t shift = CalcShift(TPageSize);

            static constexpr uint32_t mask = TPageSize - 1;

            TEntry** _pages = nullptr;

            uint32_t _capacity = 0;

            static_assert((TPageSize& (TPageSize - 1)) == 0, "TPageSize must be a power of two");

        public:

            static constexpr uint32_t PageSize = (TPageSize != 0) ? TPageSize : 4096;

            EntryArray() = default;

            ~EntryArray()
            {
                if (_pages)
                {
                    if constexpr (TPageSize > 0)
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
                if constexpr (TPageSize > 0)
                    return _pages[index >> shift][index & mask];
                else
                    return *_pages[index];
            }

            __forceinline const TEntry& operator[](uint64_t index) const
            {
                if constexpr (TPageSize > 0)
                    return _pages[index >> shift][index & mask];
                else
                    return *_pages[index];
            }

            void AdjustSize(uint32_t size)
            {
                if constexpr (TPageSize > 0)
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

        template<typename TEntry, uint32_t TPageSize>
        class IndexArray : public EntryArray<TEntry, TPageSize>
        {
        public:
            EntryArray<uint32_t, TPageSize> _index;
        };

        enum class Mode { Fast = 0, FastDivMod = 1, SaveMemoryFast = 2, SaveMemoryOpt = 4, SaveMemoryMax = 8 };

        template <typename EntryType, bool isIndex>
        struct EntryArrayType;

        template <typename EntryType>
        struct EntryArrayType<EntryType, true>
        {
            using Type = IndexArray<EntryType, 4096>;
        };

        template <typename EntryType>
        struct EntryArrayType<EntryType, false>
        {
            using Type = EntryArray<EntryType, 4096>;
        };

        template <typename TKey, typename TValue, Type type, class Hash, Mode mode = Mode::Fast, bool bFix = false>
        class Core
        {
            using TagVector = TagVectorCore;

            using EntryType = Entry<TKey, TValue, type == Type::Map>;

            using EntryArrayType = typename EntryArrayType<EntryType, type == Type::Index>::Type;

            using TagArrayType = TagArray<TagVector>;

        protected:

            TagArrayType _tags;

            EntryArrayType _entries;

            const Hash _hasher;

        public:
            static constexpr uint32_t MIN_SIZE = 4096;
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

        protected:

            void Rehash()
            {
                if (_Count != 0)
                {
                    RehashInternal(_tags.size());
                }
            }

        private:

            void RehashInternal(uint32_t size)
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

                        _entries._index[tupleIndex] = realIndex;
                    }
                }
                else
                {
                    TagArray prevTags(std::move(_tags));

                    _tags.AdjustSize(size); _tags.Init();

                    const auto prevCount = _Count; _Count = 0;
                    
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

        public:

            void Resize(uint32_t size)
            {
                if (_Capacity > AdjustCapacity(size)) return;

                InitCapacity(size);

                if (_Capacity == _tags.size()) return;

                _entries.AdjustSize(_Capacity);

                if constexpr (type == Type::Index)
                {
                    _entries._index.AdjustSize(_Capacity);
                }

                if (_Count == 0)
                {
                    _tags.AdjustSize(_Capacity); _tags.Init();
                }
                else
                {
                    RehashInternal(_Capacity);
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
                            const auto realIndex = _entries._index[tupleIndex + TrailingZeroCount<bFix>(resultMask)];

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

                uint64_t emptyMask;

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
                            const auto entryIndex = tupleIndex + TrailingZeroCount<bFix>(resultMask);

                            if constexpr (type == Type::Index)
                            {
                                const auto realIndex = _entries._index[entryIndex];

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

                        tupleIndex = AdjustTupleIndex(tupleIndex + (jump += TagVector::SIZE));
                    }
                }
                else
                {
                    while (true)
                    {
                        source.Load(_tags.data() + tupleIndex);

                        if (emptyMask = source.GetEmptyOrTomeStoneMask()) break;

                        tupleIndex = AdjustTupleIndex(tupleIndex + (jump += TagVector::SIZE));
                    }
                }

                const auto entryIndex = tupleIndex + TrailingZeroCount<bFix>(emptyMask);

                _tags[entryIndex] = tag;

                if constexpr (type == Type::Index)
                {
                    const auto realIndex = _Count;

                    _entries._index[entryIndex] = realIndex;

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

            class ConstIterator 
            {
                using TagVector = TagVectorIterator;

            public:

                ConstIterator(const Core* corePtr, bool bBegin) : _corePtr(corePtr)
                {
                    if constexpr (type == Type::Index)
                    {
                        if (!bBegin)
                        {
                            _idx = _corePtr->Count();
                        }
                    }
                    else
                    {
                        if (_corePtr->Count())
                        {
                            if (bBegin)
                            {
                                _mask = TagVector::GetNonEmptyMask<false>(_corePtr->_tags.data()); Seek();
                            }
                            else
                            {
                                _idx = _base = _corePtr->Capacity();
                            }
                        }
                    }
                }
                
                const EntryType& operator*() const
                {
                    return _corePtr->_entries[_idx];
                }

                ConstIterator& operator++()
                {
                    if constexpr (type != Type::Index)
                    {
                        Seek(); return *this;
                    }
                    else
                    {
                        _idx++; return *this;
                    }
                }

                bool operator==(const ConstIterator& other) const
                {
                    return _idx == other._idx;
                }

                bool operator!=(const ConstIterator& other) const
                {
                    return _idx != other._idx;
                }

            private:

                const Core* _corePtr;

                uint32_t _idx = 0, _base = 0;

                uint64_t _mask = 0;

                __forceinline void Seek()
                {
                    static_assert(type != Type::Index);

                    while (true)
                    {
                        while (_mask)
                        {
                            _idx = _base + TrailingZeroCount<false>(_mask);

                            _mask = ResetLowestSetBit(_mask); return;
                        }

                        _base += TagVector::SIZE;

                        if (_base >= _corePtr->_tags.size())
                        {
                            _idx = _base; return;
                        }
                        
                        _mask = TagVector::GetNonEmptyMask<true>(_corePtr->_tags.data() + _base);
                    }
                }
            };

            friend class ConstIterator;

        public:

            ConstIterator begin() const
            {
                return ConstIterator(this, true);
            }

            ConstIterator end() const
            {
                return ConstIterator(this, false);
            }

        protected:

            Core()
            {
                Resize(MIN_SIZE);
            }

            uint32_t _Capacity = 0, _CapacityMask;
            
            uint32_t _Count = 0, _CountGrowthLimit;

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
            using core::Rehash;
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
            using core::Rehash;
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