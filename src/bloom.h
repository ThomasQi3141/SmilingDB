#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace SmilingDB {

// ~1% false positives at 10 bits per key
std::string BuildBloomFilter(const std::vector<std::string> &keys,
                             int bits_per_key);
// false -> key is definitely NOT in the set, true -> maybe
bool BloomMayContain(std::string_view filter, std::string_view key);

} // namespace SmilingDB
