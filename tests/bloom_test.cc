#include "bloom.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace SmilingDB {
namespace {

std::vector<std::string> MakeKeys(int n, const std::string &prefix) {
	std::vector<std::string> keys;
	for (int i = 0; i < n; ++i) {
		keys.push_back(prefix + std::to_string(i));
	}
	return keys;
}

double FalsePositiveRate(const std::string &filter, int probes) {
	int hits = 0;
	for (const std::string &key : MakeKeys(probes, "absent")) {
		if (BloomMayContain(filter, key)) {
			++hits;
		}
	}
	return static_cast<double>(hits) / probes;
}

TEST(BloomTest, EmptyFilterMatchesEverything) {
	EXPECT_TRUE(BloomMayContain("", "anything"));
	EXPECT_TRUE(BloomMayContain("", ""));
}

TEST(BloomTest, NoKeysMatchesNothing) {
	std::string filter = BuildBloomFilter({}, 10);
	EXPECT_FALSE(BloomMayContain(filter, "a"));
	EXPECT_FALSE(BloomMayContain(filter, ""));
}

// a bloom filter can say "maybe" wrongly, but never "no" wrongly
TEST(BloomTest, NoFalseNegatives) {
	for (int n : {1, 10, 100, 1000, 10'000}) {
		std::vector<std::string> keys = MakeKeys(n, "key");
		std::string filter = BuildBloomFilter(keys, 10);
		for (const std::string &key : keys) {
			ASSERT_TRUE(BloomMayContain(filter, key))
			    << "n " << n << " " << key;
		}
	}
}

TEST(BloomTest, FalsePositiveRateIsLow) {
	std::string filter = BuildBloomFilter(MakeKeys(10'000, "key"), 10);
	EXPECT_LT(FalsePositiveRate(filter, 10'000), 0.02);
}

TEST(BloomTest, MoreBitsFewerFalsePositives) {
	std::vector<std::string> keys = MakeKeys(10'000, "key");
	double loose = FalsePositiveRate(BuildBloomFilter(keys, 4), 10'000);
	double tight = FalsePositiveRate(BuildBloomFilter(keys, 20), 10'000);
	EXPECT_LT(tight, loose);
	EXPECT_LT(tight, 0.001);
}

TEST(BloomTest, FilterSizeScalesWithBitsPerKey) {
	std::vector<std::string> keys = MakeKeys(1000, "key");
	// 1000 keys * 10 bits = 1250 bytes, + 1 byte for k
	EXPECT_EQ(BuildBloomFilter(keys, 10).size(), 1251u);
	EXPECT_EQ(BuildBloomFilter(keys, 20).size(), 2501u);
}

TEST(BloomTest, EmptyAndBinaryKeys) {
	std::vector<std::string> keys = {"", std::string("\0", 1),
	                                 std::string("a\0b", 3), "\xff\xfe"};
	std::string filter = BuildBloomFilter(keys, 10);
	for (const std::string &key : keys) {
		EXPECT_TRUE(BloomMayContain(filter, key));
	}
}

TEST(BloomTest, Deterministic) {
	std::vector<std::string> keys = MakeKeys(500, "key");
	EXPECT_EQ(BuildBloomFilter(keys, 10), BuildBloomFilter(keys, 10));
}

// garbage k byte -> don't trust the filter, say maybe
TEST(BloomTest, BadProbeCountMatchesEverything) {
	std::string filter = BuildBloomFilter(MakeKeys(100, "key"), 10);
	filter.back() = static_cast<char>(200);
	EXPECT_TRUE(BloomMayContain(filter, "definitely not in there"));
}

} // namespace
} // namespace SmilingDB
