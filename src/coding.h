#pragma once

#include <cstdint>
#include <string>

namespace SmilingDB {

void PutFixed32(std::string &dst, uint32_t value); // appends 4 bytes
void PutFixed64(std::string &dst, uint64_t value); // appends 8 bytes

uint32_t DecodeFixed32(const char *src); // reads 4 bytes
uint64_t DecodeFixed64(const char *src); // reads 8 bytes

} // namespace SmilingDB
