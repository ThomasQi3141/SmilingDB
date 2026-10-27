#pragma once

#include "memtable.h"

#include <expected>
#include <filesystem>
#include <string>

namespace SmilingDB {

// replays every record into mem, torn last record (crashed mid write) gets
// skipped, bad crc -> error
std::expected<void, std::string> ReplayLog(const std::filesystem::path &path,
                                           MemTable &mem);

} // namespace SmilingDB
