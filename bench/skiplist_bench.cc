#include "skiplist.h"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <random>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

// thin adapters so every benchmark body is shared across containers
struct SkipListAdapter {
	SkipList impl;
	void Insert(const std::string &k) { impl.Add(k); }
	bool Contains(const std::string &k) const { return impl.Search(k); }
	bool Erase(const std::string &k) { return impl.Erase(k); }
};

struct StdSetAdapter {
	std::set<std::string> impl;
	void Insert(const std::string &k) { impl.insert(k); }
	bool Contains(const std::string &k) const { return impl.count(k) != 0; }
	bool Erase(const std::string &k) { return impl.erase(k) != 0; }
};

struct StdUnorderedSetAdapter {
	std::unordered_set<std::string> impl;
	void Insert(const std::string &k) { impl.insert(k); }
	bool Contains(const std::string &k) const { return impl.count(k) != 0; }
	bool Erase(const std::string &k) { return impl.erase(k) != 0; }
};

constexpr int kKeyLength = 16;

// deterministic random keys so all containers see the same workload
std::vector<std::string> MakeKeys(size_t n, uint32_t seed) {
	std::mt19937 rng(seed);
	std::uniform_int_distribution<int> ch('a', 'z');
	std::vector<std::string> keys(n);
	for (std::string &k : keys) {
		k.resize(kKeyLength);
		for (char &c : k) {
			c = static_cast<char>(ch(rng));
		}
	}
	return keys;
}

template <typename Container> void BM_Insert(benchmark::State &state) {
	std::vector<std::string> keys = MakeKeys(state.range(0), 1);
	for (auto _ : state) {
		Container c;
		for (const std::string &k : keys) {
			c.Insert(k);
		}
		benchmark::DoNotOptimize(c);
	}
	state.SetItemsProcessed(state.iterations() * keys.size());
}

template <typename Container> void BM_LookupHit(benchmark::State &state) {
	std::vector<std::string> keys = MakeKeys(state.range(0), 1);
	Container c;
	for (const std::string &k : keys) {
		c.Insert(k);
	}
	// probe in a different order than insertion
	std::shuffle(keys.begin(), keys.end(), std::mt19937(2));
	for (auto _ : state) {
		for (const std::string &k : keys) {
			benchmark::DoNotOptimize(c.Contains(k));
		}
	}
	state.SetItemsProcessed(state.iterations() * keys.size());
}

template <typename Container> void BM_LookupMiss(benchmark::State &state) {
	std::vector<std::string> keys = MakeKeys(state.range(0), 1);
	std::vector<std::string> misses = MakeKeys(state.range(0), 3);
	Container c;
	for (const std::string &k : keys) {
		c.Insert(k);
	}
	for (auto _ : state) {
		for (const std::string &k : misses) {
			benchmark::DoNotOptimize(c.Contains(k));
		}
	}
	state.SetItemsProcessed(state.iterations() * misses.size());
}

template <typename Container> void BM_Erase(benchmark::State &state) {
	std::vector<std::string> keys = MakeKeys(state.range(0), 1);
	std::vector<std::string> order = keys;
	std::shuffle(order.begin(), order.end(), std::mt19937(2));
	for (auto _ : state) {
		state.PauseTiming();
		Container c;
		for (const std::string &k : keys) {
			c.Insert(k);
		}
		state.ResumeTiming();
		for (const std::string &k : order) {
			benchmark::DoNotOptimize(c.Erase(k));
		}
	}
	state.SetItemsProcessed(state.iterations() * keys.size());
}

#define SMILINGDB_BENCH_ALL(fn)                                                \
	BENCHMARK_TEMPLATE(fn, SkipListAdapter)                                    \
	    ->RangeMultiplier(8)                                                   \
	    ->Range(1 << 10, 1 << 19);                                             \
	BENCHMARK_TEMPLATE(fn, StdSetAdapter)                                      \
	    ->RangeMultiplier(8)                                                   \
	    ->Range(1 << 10, 1 << 19);                                             \
	BENCHMARK_TEMPLATE(fn, StdUnorderedSetAdapter)                             \
	    ->RangeMultiplier(8)                                                   \
	    ->Range(1 << 10, 1 << 19)

SMILINGDB_BENCH_ALL(BM_Insert);
SMILINGDB_BENCH_ALL(BM_LookupHit);
SMILINGDB_BENCH_ALL(BM_LookupMiss);
SMILINGDB_BENCH_ALL(BM_Erase);

} // namespace
