#pragma once

#include <cstdint>
#include <string_view>

namespace SmilingDB {
// returns the crc-32 checksum of `data`
uint32_t Crc32(std::string_view data);

} // namespace SmilingDB