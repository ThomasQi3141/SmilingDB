#include "bloom.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace SmilingDB {

namespace {

// fnv-1a + a final mix so both 32 bit halves are well spread
uint64_t BloomHash(std::string_view key) {
	uint64_t h = 0xcbf29ce484222325;
	for (unsigned char c : key) {
		h ^= c;
		h *= 0x100000001b3;
	}
	h ^= h >> 33;
	h *= 0xff51afd7ed558ccd;
	h ^= h >> 33;
	return h;
}

} // namespace

std::string BuildBloomFilter(const std::vector<std::string> &keys,
                             int bits_per_key) {
	// k = bits_per_key * ln(2) gives the lowest false positive rate
	int k = std::clamp(static_cast<int>(bits_per_key * 0.69), 1, 30);
	size_t bits = std::max<size_t>(64, keys.size() * bits_per_key);
	size_t bytes = (bits + 7) / 8;
	bits = bytes * 8;

	std::string filter(bytes, '\0');
	for (const std::string &key : keys) {
		uint64_t h = BloomHash(key);
		uint32_t h1 = static_cast<uint32_t>(h);
		uint32_t h2 = static_cast<uint32_t>(h >> 32);
		// double hashing: k probes from just 2 hashes
		for (int i = 0; i < k; ++i) {
			size_t bit = h1 % bits;
			filter[bit / 8] |= static_cast<char>(1 << (bit % 8));
			h1 += h2;
		}
	}
	// k goes in the last byte so the reader knows how many probes to do
	filter.push_back(static_cast<char>(k));
	return filter;
}

bool BloomMayContain(std::string_view filter, std::string_view key) {
	// no filter -> have to assume it could be there
	if (filter.size() < 2) {
		return true;
	}
	int k = static_cast<uint8_t>(filter.back());
	if (k < 1 || k > 30) {
		return true;
	}
	size_t bits = (filter.size() - 1) * 8;
	uint64_t h = BloomHash(key);
	uint32_t h1 = static_cast<uint32_t>(h);
	uint32_t h2 = static_cast<uint32_t>(h >> 32);
	for (int i = 0; i < k; ++i) {
		size_t bit = h1 % bits;
		if ((static_cast<uint8_t>(filter[bit / 8]) & (1 << (bit % 8))) == 0) {
			return false;
		}
		h1 += h2;
	}
	return true;
}

} // namespace SmilingDB
