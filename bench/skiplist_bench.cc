#include "skiplist.h"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <map>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

// thin adapters so every benchmark body is shared across containers. all of
// them store the same Entry type so value copies cost the same everywhere
struct SkipListAdapter {
	SkipList impl;
	void Put(const std::string &k, const Entry &v) { impl.Add(k, v); }
	const Entry *Get(const std::string &k) const { return impl.Search(k); }
	bool Erase(const std::string &k) { return impl.Erase(k); }
	size_t Scan() const {
		size_t bytes = 0;
		for (auto it = impl.begin(); it != impl.end(); ++it) {
			bytes += it.value().value.size();
		}
		return bytes;
	}
};

template <typename Map> struct StdMapAdapterBase {
	Map impl;
	void Put(const std::string &k, const Entry &v) {
		impl.insert_or_assign(k, v);
	}
	const Entry *Get(const std::string &k) const {
		auto it = impl.find(k);
		return it == impl.end() ? nullptr : &it->second;
	}
	bool Erase(const std::string &k) { return impl.erase(k) != 0; }
	size_t Scan() const {
		size_t bytes = 0;
		for (const auto &[k, v] : impl) {
			bytes += v.value.size();
		}
		return bytes;
	}
};

using StdMapAdapter = StdMapAdapterBase<std::map<std::string, Entry>>;
// unordered baseline: not a drop-in for a memtable (no ordered scan), but shows
// the cost of keeping keys sorted at all
using StdUnorderedMapAdapter =
    StdMapAdapterBase<std::unordered_map<std::string, Entry>>;

constexpr int kKeyLength = 16;
constexpr int kValueLength = 100;

// deterministic random strings so all containers see the same workload
std::vector<std::string> MakeStrings(size_t n, int length, uint32_t seed) {
	std::mt19937 rng(seed);
	std::uniform_int_distribution<int> ch('a', 'z');
	std::vector<std::string> out(n);
	for (std::string &s : out) {
		s.resize(length);
		for (char &c : s) {
			c = static_cast<char>(ch(rng));
		}
	}
	return out;
}

std::vector<std::string> MakeKeys(size_t n, uint32_t seed) {
	return MakeStrings(n, kKeyLength, seed);
}

std::vector<Entry> MakeValues(size_t n, uint32_t seed) {
	std::vector<Entry> values;
	values.reserve(n);
	for (std::string &s : MakeStrings(n, kValueLength, seed)) {
		values.push_back(Entry{std::move(s), false});
	}
	return values;
}

template <typename Container>
void Fill(Container &c, const std::vector<std::string> &keys,
          const std::vector<Entry> &values) {
	for (size_t i = 0; i < keys.size(); ++i) {
		c.Put(keys[i], values[i]);
	}
}

template <typename Container> void BM_Insert(benchmark::State &state) {
	std::vector<std::string> keys = MakeKeys(state.range(0), 1);
	std::vector<Entry> values = MakeValues(state.range(0), 10);
	for (auto _ : state) {
		Container c;
		Fill(c, keys, values);
		benchmark::DoNotOptimize(c);
	}
	state.SetItemsProcessed(state.iterations() * keys.size());
}

// every Put hits an existing key and replaces its value
template <typename Container> void BM_Overwrite(benchmark::State &state) {
	std::vector<std::string> keys = MakeKeys(state.range(0), 1);
	std::vector<Entry> values = MakeValues(state.range(0), 10);
	std::vector<Entry> new_values = MakeValues(state.range(0), 11);
	Container c;
	Fill(c, keys, values);
	std::shuffle(keys.begin(), keys.end(), std::mt19937(2));
	for (auto _ : state) {
		for (size_t i = 0; i < keys.size(); ++i) {
			c.Put(keys[i], new_values[i]);
		}
	}
	state.SetItemsProcessed(state.iterations() * keys.size());
}

template <typename Container> void BM_LookupHit(benchmark::State &state) {
	std::vector<std::string> keys = MakeKeys(state.range(0), 1);
	std::vector<Entry> values = MakeValues(state.range(0), 10);
	Container c;
	Fill(c, keys, values);
	// probe in a different order than insertion
	std::shuffle(keys.begin(), keys.end(), std::mt19937(2));
	for (auto _ : state) {
		for (const std::string &k : keys) {
			benchmark::DoNotOptimize(c.Get(k));
		}
	}
	state.SetItemsProcessed(state.iterations() * keys.size());
}

template <typename Container> void BM_LookupMiss(benchmark::State &state) {
	std::vector<std::string> keys = MakeKeys(state.range(0), 1);
	std::vector<Entry> values = MakeValues(state.range(0), 10);
	std::vector<std::string> misses = MakeKeys(state.range(0), 3);
	Container c;
	Fill(c, keys, values);
	for (auto _ : state) {
		for (const std::string &k : misses) {
			benchmark::DoNotOptimize(c.Get(k));
		}
	}
	state.SetItemsProcessed(state.iterations() * misses.size());
}

template <typename Container> void BM_Erase(benchmark::State &state) {
	std::vector<std::string> keys = MakeKeys(state.range(0), 1);
	std::vector<Entry> values = MakeValues(state.range(0), 10);
	std::vector<std::string> order = keys;
	std::shuffle(order.begin(), order.end(), std::mt19937(2));
	for (auto _ : state) {
		state.PauseTiming();
		Container c;
		Fill(c, keys, values);
		state.ResumeTiming();
		for (const std::string &k : order) {
			benchmark::DoNotOptimize(c.Erase(k));
		}
	}
	state.SetItemsProcessed(state.iterations() * keys.size());
}

// full iteration over every entry (what a memtable flush does)
template <typename Container> void BM_Scan(benchmark::State &state) {
	std::vector<std::string> keys = MakeKeys(state.range(0), 1);
	std::vector<Entry> values = MakeValues(state.range(0), 10);
	Container c;
	Fill(c, keys, values);
	for (auto _ : state) {
		benchmark::DoNotOptimize(c.Scan());
	}
	state.SetItemsProcessed(state.iterations() * keys.size());
}

#define SMILINGDB_BENCH_ALL(fn)                                                \
	BENCHMARK_TEMPLATE(fn, SkipListAdapter)                                    \
	    ->RangeMultiplier(8)                                                   \
	    ->Range(1 << 10, 1 << 19);                                             \
	BENCHMARK_TEMPLATE(fn, StdMapAdapter)                                      \
	    ->RangeMultiplier(8)                                                   \
	    ->Range(1 << 10, 1 << 19);                                             \
	BENCHMARK_TEMPLATE(fn, StdUnorderedMapAdapter)                             \
	    ->RangeMultiplier(8)                                                   \
	    ->Range(1 << 10, 1 << 19)

SMILINGDB_BENCH_ALL(BM_Insert);
SMILINGDB_BENCH_ALL(BM_Overwrite);
SMILINGDB_BENCH_ALL(BM_LookupHit);
SMILINGDB_BENCH_ALL(BM_LookupMiss);
SMILINGDB_BENCH_ALL(BM_Erase);
SMILINGDB_BENCH_ALL(BM_Scan);

} // namespace
