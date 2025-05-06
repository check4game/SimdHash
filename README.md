# SimdHash

MZ::SimdHash:Map is now the fastest hashmap for uint64_t/uint32_t

```
SimdHash, build: 1024, Date: May  6 2025 20:37:16
Intel(R) Core(TM) i7-10700K CPU @ 3.80GHz, Comet Lake (Core i7), 8/16, 3.792GHz

SimdHash.exe run [max [min [step]]] [-simdhm|-simdhs|-simdhi|-abslfhm|-em7hm|-em8hm]

-simdhm MZ::SimdHash::Map, -simdhs MZ::SimdHash::Set, -simdhi MZ::SimdHash::Index
-tslrs tsl::robin_set, -tslrm tsl::robin_man
-abslfhs absl::flat_hash_set, -abslfhm absl::flat_hash_map
-em7hm emhash7::Map, -em8hm emhash8::HashMap
-ankerlhm ankerl::unordered_dense::map
-stdhash, -abslhash, -simdhash, -ankerlhash, -abslhash by default
-reuse, by default create a new...
-rmin, -ravg, -rmax, call reserve before use
-32, -64 key size in bits, -64 by default
-unique ::AddUnique, by default ::Add
-seq, sequential set of numbers
-shuffle, random shuffle
-presult, results for python to std::cerr
-test1, only add/insert/emplace
-test2, remove(2%+2%), contains(4%+4%), add(2%+2%)
-test3, contains(15% load)
-test4, contains(15% load reverse)
-test5, const iterator

SimdHash.exe rnd [32|64|128|256]
32|64|128|256  dataset size in MB, 128 by default

SimdHash.exe selftest
run set of internal tests
```
## add key/value to empty hashmap
![add key/value to empty hashmap](/results/11500/test1_128_reuse_random.png)

## add key/value to new hashmap
![add key/value to new hashmap](/results/11500/test1_128_random.png)
