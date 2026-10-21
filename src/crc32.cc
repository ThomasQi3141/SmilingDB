#include "crc32.h"

#include <array>

namespace SmilingDB {
// unnamed namespace = internal linkage, only visible in this .cpp
namespace {
// CRC-32 -> polynomial 0x04C11DB7 (bit reversed) so we can do LSB-first
constexpr uint32_t kPolynomial = 0xEDB88320;

constexpr std::array<uint32_t, 256> MakeTable() {
	std::array<uint32_t, 256> table{};
	for (uint32_t n = 0; n < 256; ++n) {
		uint32_t c = n;
		for (int k = 0; k < 8; ++k) {
			if (c & 1) {
				c = (c >> 1) ^ kPolynomial;
			} else {
				c = c >> 1;
			}
		}
		table[n] = c;
	}
	return table;
}
constexpr std::array<uint32_t, 256> kTable = MakeTable();
} // namespace

uint32_t Crc32(std::string_view data) {
	uint32_t crc = 0xFFFFFFFF;
	for (unsigned char byte : data) {
		crc = kTable[(crc ^ byte) & 0xFF] ^ (crc >> 8);
	}
	return crc ^ 0xFFFFFFFF;
}
} // namespace SmilingDB