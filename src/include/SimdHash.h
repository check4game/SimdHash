#ifndef __SIMDHASH_H__
#define __SIMDHASH_H__

#include <intrin.h>
#include <malloc.h>

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

namespace MZ
{
    namespace SimdHash
    {
        static constexpr uint32_t Build = 1024;
       
        template <typename TMask>
        static __forceinline const TMask ResetLowestSetBit(const TMask mask)
        {
            static_assert(
                std::is_same_v<TMask, uint32_t> || std::is_same_v<TMask, uint64_t>,
                "Template parameter must be either uint32_t or uint64_t"
                );

            return mask & (mask - 1);
        }

        template <bool bFix, typename TMask>
        static __forceinline const uint32_t TrailingZeroCount(TMask mask)
        {
            static_assert(
                std::is_same_v<TMask, uint32_t> || std::is_same_v<TMask, uint64_t>,
                "Template parameter must be either uint32_t or uint64_t"
                );

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

                if (mask & 0x0100) return 8;
                if (mask & 0x0200) return 9;
                if (mask & 0x0400) return 10;
                if (mask & 0x0800) return 11;
                if (mask & 0x0800) return 12;
                if (mask & 0x2000) return 13;
                if (mask & 0x4000) return 14;
                if (mask & 0x8000) return 15;
            }

            if constexpr (std::is_same_v<TMask, uint32_t>)
                return _tzcnt_u32(mask);
            else
                return static_cast<uint32_t>(_tzcnt_u64(mask));
        }

        inline static const uint64_t COMPILE_TIME_SEED = reinterpret_cast<uintptr_t>(&COMPILE_TIME_SEED);

        enum class HashType { Default, Fib, Absl};

        template <typename TKey, HashType type = HashType::Absl>
        struct Hash
        {
            static constexpr auto fib = UINT64_C(11400714819323198485);

            static constexpr auto kMul = UINT64_C(0xdcb22ca68cb134ed);
            
            __forceinline static uint64_t hash(const uint64_t key)
            {
                if constexpr (type == HashType::Absl)
                {
                    return (key ^ kMul) * kMul;
                }
                else
                {
                    return (key ^ fib) * fib;
                }
            }

            __forceinline uint64_t operator()(const TKey& key) const noexcept
            {
                if constexpr (std::is_integral_v<TKey>)
                {
                    return hash(static_cast<uint64_t>(key));
                }
                else if constexpr (std::is_same_v<TKey, float>)
                {
                    return hash(static_cast<uint64_t>(*reinterpret_cast<const uint32_t*>(&key)));
                }
                else if constexpr (std::is_same_v<TKey, double>)
                {
                    return hash(*reinterpret_cast<uint64_t*>(&key));
                }
                else if constexpr (std::is_pointer_v<TKey>)
                {
                    return hash(reinterpret_cast<uint64_t>(key));
                }
                else
                {
                    return std::hash<TKey>()(key);
                }
            }
        };

        static __forceinline uint32_t RoundUpToPowerOf2(uint32_t value)
        {
            if (!value || !ResetLowestSetBit(value)) return value;

            value--;

            value |= value >> 1;
            value |= value >> 2;
            value |= value >> 4;
            value |= value >> 8;
            value |= value >> 16;

            return ++value;
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
        public:

            using MaskType = typename TagVectorType<TagVectorSize>::MaskType;

        private:

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

            enum class Mode { Default, Align, Stream };

            template <Mode mode = Mode::Default>
            __forceinline static TagVectorType LoadVector(const uint8_t* ptr)
            {
                if constexpr (TagVectorSize == 16)
                {
                    if constexpr (mode == Mode::Align)
                        return _mm_load_si128((const __m128i*)ptr);
                    else
                        return _mm_loadu_si128((const __m128i*)ptr);
                }
                else if constexpr (TagVectorSize == 32)
                {
                    if constexpr (mode == Mode::Align)
                        return _mm256_load_si256((const __m256i*)ptr);
                    else
                        return _mm256_loadu_si256((const __m256i*)ptr);
                }
                else
                {
                    if constexpr (mode == Mode::Align)
                        return _mm512_load_si512((const __m512i*)ptr);
                    else
                        return _mm512_loadu_si512((const __m512i*)ptr);
                }
            }

            template <Mode mode = Mode::Default>
            __forceinline void Load(const uint8_t* ptr)
            {
                xmm = LoadVector<mode>(ptr);
            }

            template <Mode mode = Mode::Default>
            __forceinline void Store(uint8_t* ptr) const
            {
                if constexpr (mode == Mode::Stream)
                {
                    if constexpr (TagVectorSize == 16)
                        _mm_stream_si128((__m128i*)ptr, xmm);
                    else if constexpr (TagVectorSize == 32)
                        _mm256_stream_si256((__m256i*)ptr, xmm);
                    else
                        _mm512_stream_si512((__m256i*)ptr, xmm);
                }
                else if constexpr (mode == Mode::Align)
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

            __forceinline MaskType GetEmptyMask() const
            {
                return GetEmptyMask(xmm);
            }

            __forceinline static MaskType GetEmptyMask(const uint8_t* ptr)
            {
                return GetEmptyMask(LoadVector(ptr));
            }

            __forceinline static MaskType GetEmptyMask(const TagVectorType& xmm)
            {
                if constexpr (TagVectorSize == 16)
                {
                    return static_cast<MaskType>(_mm_movemask_epi8(_mm_cmpeq_epi8(EMPTY_VECTOR, xmm)));
                }
                else if constexpr (TagVectorSize == 32)
                {
                    return static_cast<MaskType>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(EMPTY_VECTOR, xmm)));
                }
                else
                {
                    return static_cast<MaskType>(_mm512_cmp_epi8_mask(EMPTY_VECTOR, xmm, _MM_CMPINT_EQ));
                }
            }

            __forceinline MaskType GetEmptyOrTomeStoneMask() const
            {
                if constexpr (TagVectorSize == 16)
                {
                    return static_cast<MaskType>(_mm_movemask_epi8
                    (_mm_cmpeq_epi8(EMPTY_VECTOR, _mm_and_si128(FORBIDDEN_VECTOR, xmm))));
                }
                else if constexpr (TagVectorSize == 32)
                {
                    return static_cast<MaskType>(_mm256_movemask_epi8
                    (_mm256_cmpeq_epi8(EMPTY_VECTOR, _mm256_and_si256(FORBIDDEN_VECTOR, xmm))));
                }
                else
                {
                    return static_cast<MaskType>(_mm512_cmp_epi8_mask(EMPTY_VECTOR,
                        _mm512_and_si512(FORBIDDEN_VECTOR, xmm), _MM_CMPINT_EQ));
                }
            }

            template <Mode mode = Mode::Default>
            __forceinline static MaskType GetNonEmptyMask(const uint8_t* ptr)
            {
                if constexpr (TagVectorSize == 16)
                {
                    return static_cast<MaskType>
                        (_mm_movemask_epi8(_mm_cmpeq_epi8(ZERO_VECTOR,
                            _mm_and_si128(EMPTY_VECTOR, LoadVector<mode>(ptr)))));
                }
                else if constexpr (TagVectorSize == 32)
                {
                    return static_cast<MaskType>
                        (_mm256_movemask_epi8(_mm256_cmpeq_epi8(ZERO_VECTOR,
                            _mm256_and_si256(EMPTY_VECTOR, LoadVector<mode>(ptr)))));
                }
                else
                {
                    return static_cast<MaskType>
                        (_mm512_cmp_epi8_mask(ZERO_VECTOR,
                            _mm512_and_si512(EMPTY_VECTOR, LoadVector<mode>(ptr)), _MM_CMPINT_EQ));
                }
            }

            __forceinline MaskType GetCmpMask(const TagVector& vector) const
            {
                if constexpr (TagVectorSize == 16)
                    return static_cast<MaskType>(_mm_movemask_epi8(_mm_cmpeq_epi8(vector, xmm)));
                else if constexpr (TagVectorSize == 32)
                    return static_cast<MaskType>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(vector, xmm)));
                else
                    return static_cast<MaskType>(_mm512_cmp_epi8_mask(vector, xmm, _MM_CMPINT_EQ));
            }

            static const TagVector EMPTY_VECTOR, ZERO_VECTOR, FORBIDDEN_VECTOR;

            static constexpr uint8_t SIZE = sizeof(xmm), MAX_SIZE = sizeof(__m512i);

            static constexpr uint8_t EMPTY = 0x80, TOMBSTONE = 0x81, FORBIDDEN = 0x82, ZERO = 0x00;
        };

        using TagVectorCore = TagVector<16>;

        const TagVectorCore TagVectorCore::EMPTY_VECTOR = TagVectorCore(TagVectorCore::EMPTY);
        const TagVectorCore TagVectorCore::ZERO_VECTOR = TagVectorCore(TagVectorCore::ZERO);
        const TagVectorCore TagVectorCore::FORBIDDEN_VECTOR = TagVectorCore(TagVectorCore::FORBIDDEN);

#ifdef __AVX2__

    #ifdef __AVX512F__
        using TagVectorIterator = TagVector<64>;
    #else
        using TagVectorIterator = TagVector<32>;
    #endif

    const TagVectorIterator TagVectorIterator::EMPTY_VECTOR = TagVectorIterator(TagVectorIterator::EMPTY);
    const TagVectorIterator TagVectorIterator::ZERO_VECTOR = TagVectorIterator(TagVectorIterator::ZERO);

#else

    using TagVectorIterator = TagVector<16>;

#endif

        enum class Type { Map, Set, Index };

#pragma pack(push, 1)

        template<typename TKey, typename TValue, bool IsMap>
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
                        TagVector::EMPTY_VECTOR.Store<TagVector::Mode::Stream>(ptr);
                    }

                    TagVector::FORBIDDEN_VECTOR.Store<TagVector::Mode::Stream>(end());
                }
            }

            uint32_t size() const { return _size; }

            void AdjustSize(uint32_t size)
            {
                assert(size > _size);

                if (_ptr) Clear();

                _ptr = static_cast<uint8_t*>(_aligned_malloc(size + TagVector::SIZE, TagVector::MAX_SIZE));

                assert(nullptr != _ptr);

                _size = size;
            }

        protected:

            uint32_t _size = 0;

            uint8_t* _ptr = nullptr;
        };

        template<typename TEntry, uint32_t Shift = 12>
        class EntryArray
        {
            static constexpr uint32_t PageSize = 1 << Shift, Mask = (1 << Shift) - 1;

            TEntry** _pages = nullptr;

            uint32_t _size = 0;

            static_assert(Shift >= 10 && Shift <= 14, "Shift must be [10..14]");

        public:

            uint32_t size() const
            {
                return _size;
            }

            constexpr uint32_t GetPageSize() const
            {
                return PageSize;
            }

            EntryArray() = default;

            ~EntryArray()
            {
                if (_pages)
                {
                    for (uint32_t i = 0; i < _size / PageSize; i++)
                    {
                        delete[] _pages[i];
                    }

                    delete[] _pages; _pages = nullptr;
                }
            }

            __forceinline TEntry& operator[](uint64_t index)
            {
                return _pages[index >> Shift][index & Mask];
            }

            __forceinline const TEntry& operator[](uint64_t index) const
            {
                return _pages[index >> Shift][index & Mask];
            }

            template<bool bUse>
            void AdjustSize(uint32_t size)
            {
                if constexpr (bUse)
                {
                    if ((size % PageSize) != 0)
                    {
                        size += (PageSize - (size % PageSize));
                    }

                    assert(size > _size);

                    auto old_pages = _pages;
                    auto old_capacity = _size / PageSize;

                    _pages = new TEntry * [size / PageSize];

                    for (uint32_t i = 0; i < size / PageSize; i++)
                    {
                        if (i < old_capacity) _pages[i] = old_pages[i]; else _pages[i] = new TEntry[PageSize];
                    }

                    if (old_pages) delete[] old_pages;

                    _size = size;
                }
            }
        };

        template<typename TEntry, uint32_t Shift>
        class IndexArray : public EntryArray<TEntry, Shift>
        {
        public:
            EntryArray<uint32_t, Shift> realIndex;
        };

        template <typename TKey, typename TValue, Type type>
        struct EntryArrayType;

        template <typename TKey, typename TValue>
        struct EntryArrayType<TKey, TValue, Type::Index>
        {
            using EntryType = typename Entry<TKey, void, false>;
            using Type = IndexArray<EntryType, 12>;
        };

        template <typename TKey, typename TValue>
        struct EntryArrayType<TKey, TValue, Type::Set>
        {
            using EntryType = typename Entry<TKey, TValue, false>;
            using Type = EntryArray<EntryType, 12>;
        };

        template <typename TKey, typename TValue>
        struct EntryArrayType<TKey, TValue, Type::Map>
        {
            using EntryType = typename Entry<TKey, TValue, true>;
            using Type = EntryArray<EntryType, 12>;
        };

        enum class Mode { Fast = 0, FastDivMod = 1, SaveMemoryFast = 2, SaveMemoryOpt = 4, SaveMemoryMax = 8, ResizeOnlyEmpty = 16 };

        template <typename TKey, typename TValue, Type type, class Hash, Mode mode = Mode::Fast, bool bFix = false>
        class Core
        {
            using TagVector = TagVectorCore;

            using MaskType = typename TagVector::MaskType;

            using EntryType = typename EntryArrayType<TKey, TValue, type>::EntryType;

            using EntryArrayType = typename EntryArrayType<TKey, TValue, type>::Type;

            using TagArrayType = typename TagArray<TagVector>;

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

                        _entries.realIndex[tupleIndex] = realIndex;
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

                if constexpr (mode == Mode::ResizeOnlyEmpty)
                {
                    assert(_Count == 0);
                }

                InitCapacity(size);

                if (_Capacity == _tags.size()) return;

                if constexpr (type == Type::Index)
                {
                    _entries.realIndex.AdjustSize<true>(_Capacity);
                }
                else
                {
                    _entries.AdjustSize<true>(_Capacity);
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

                if constexpr (mode == Mode::ResizeOnlyEmpty)
                {
                    auto new_size = static_cast<uint64_t>(size / MAX_LOAD_FACTOR);

                    if (new_size >= MAX_SIZE) return MAX_SIZE;

                    if ((new_size % _entries.GetPageSize()) != 0)
                    {
                        new_size = (new_size / _entries.GetPageSize() + 1) * _entries.GetPageSize();
                    }

                    if (new_size >= MAX_SIZE) return MAX_SIZE;

                    return static_cast<uint32_t>(new_size);
                }
                else if constexpr (mode > Mode::FastDivMod)
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
                        
                        new_size = new_size / _entries.GetPageSize() * _entries.GetPageSize();

                        if (size <= new_size) return static_cast<uint32_t>(new_size);
                    }
                }

                return RoundUpToPowerOf2(size);
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

        public:

            __forceinline bool Contains(const TKey& key) const
            {
                return FindEntry<false>(key, [](const auto&) {});
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

                const TagVector target(HashToTag(tupleIndex));

                tupleIndex = AdjustTupleIndex(tupleIndex);

                auto jump = static_cast<uint8_t>(0);

                TagVector source;

                while (true)
                {
                    source.Load(_tags.data() + tupleIndex);

                    auto resultMask = source.GetCmpMask(target);

                    while (resultMask)
                    {
                        if constexpr (type == Type::Index)
                        {
                            const auto realIndex = _entries.realIndex[tupleIndex + TrailingZeroCount<bFix>(resultMask)];

                            if (key == _entries[realIndex].key)
                            {
                                FUNCTION(realIndex); return true;
                            }
                        }
                        else if constexpr (type == Type::Set)
                        {
                            const auto realIndex = tupleIndex + TrailingZeroCount<bFix>(resultMask);

                            if (key == _entries[realIndex].key)
                            {
                                FUNCTION(realIndex); return true;
                            }
                        }
                        else if constexpr (type == Type::Map)
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

                tupleIndex = AdjustTupleIndex(tupleIndex);

                MaskType emptyMask;

                TagVector source;

                if constexpr (!bUnique)
                {
                    const TagVector target(tag);

                    auto jump = static_cast<uint8_t>(0);

                    while (true)
                    {
                        source.Load(_tags.data() + tupleIndex);

                        auto resultMask = source.GetCmpMask(target);

                        while (resultMask)
                        {
                            const auto entryIndex = tupleIndex + TrailingZeroCount<bFix>(resultMask);

                            if constexpr (type == Type::Index)
                            {
                                const auto realIndex = _entries.realIndex[entryIndex];

                                if (key == _entries[realIndex].key)
                                {
                                    if constexpr (bUpdate) FUNCTION(realIndex);
                                    
                                    return false;
                                }
                            }
                            else if constexpr (type == Type::Set)
                            {
                                if (key == _entries[entryIndex].key) return false;
                            }
                            else if constexpr (type == Type::Map)
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
                    auto jump = static_cast<uint8_t>(0);

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

                    _entries.realIndex[entryIndex] = realIndex;

                    if (realIndex == _entries.size())
                    {
                        _entries.AdjustSize<true>(realIndex + 1);
                    }

                    _entries[realIndex].key = key;

                    if constexpr (bUpdate) FUNCTION(realIndex);
                }
                else if constexpr (type == Type::Set)
                {
                    _entries[entryIndex].key = key;
                }
                else if constexpr (type == Type::Map)
                {
                    auto& entry = _entries[entryIndex];
                    entry.key = key; FUNCTION(entry.value);
                }

                if (++_Count == _CountGrowthLimit) Resize(_Capacity + 1);

                return true;
            }

            __forceinline uint32_t FindEmpty(uint64_t tupleIndex) const
            {
                tupleIndex = AdjustTupleIndex(tupleIndex);

                auto jump = static_cast<uint8_t>(0);

                while(true)
                {
                    const auto emptyMask = TagVector::GetEmptyMask(_tags.data() + tupleIndex);

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

                using MaskType = typename TagVector::MaskType;

                __forceinline MaskType CalcMask()
                {
                    return TagVector::GetNonEmptyMask<TagVector::Mode::Align>(_corePtr->_tags.data() + _base);
                }

            public:

                ConstIterator(const Core* corePtr) : _corePtr(corePtr), _idx(0), _base(0)
                {
                    if constexpr (type != Type::Index)
                    {
                        if (_corePtr->Count())
                        {
                            _mask = CalcMask(); Seek();
                        }
                    }
                }

                ConstIterator(const Core* corePtr, uint32_t idx) : _corePtr(corePtr), _idx(idx), _base(idx)
                {
                    if constexpr (type == Type::Index)
                    {
                        _idx = _corePtr->Count();
                    }
                }
                
                const auto& operator*() const
                {
                    if constexpr (type == Type::Map)
                        return _corePtr->_entries[_idx];
                    else
                        return _corePtr->_entries[_idx].key;
                }

                ConstIterator& operator++()
                {
                    Seek(); return *this;
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

                uint32_t _idx, _base;

                MaskType _mask = 0;

                __forceinline void Seek()
                {
                    if constexpr (type != Type::Index)
                    {
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

                            _mask = CalcMask();
                        }
                    }
                    else
                    {
                        _idx++;
                    }
                }
            };

            friend class ConstIterator;

        public:

            ConstIterator begin() const
            {
                return ConstIterator(this);
            }

            ConstIterator end() const
            {
                return ConstIterator(this, (_Count) ? _Capacity : 0);
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
        };
    }
}

#endif