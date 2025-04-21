# SimdHash

SimdHash, build: 1023, Date: Apr 21 2025 16:50:18
Intel(R) Core(TM) i7-10700K CPU @ 3.80GHz, Comet Lake (Core i7), 8/16, 3.792GHz

SimdHash.exe run [max [min [step]]] [-simdhm|-simdhs|-simdhi|-tslrm|-abslfhm|-em7hm|-ankerlhm]

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
-test3, contains(load, 1/2 reverse)
-test4, contains(1%, 1% reverse), miss(1%)

SimdHash.exe rnd [32|64|128|256]
32|64|128|256  dataset size in MB, 128 by default

SimdHash.exe selftest
run set of internal tests
