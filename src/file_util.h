#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace SmilingDB {

std::string ErrnoMessage(std::string_view what);

// write() can do partial writes -> keep going until everything is written
std::expected<void, std::string> WriteAll(int fd, std::string_view data);
// macOS fsync doesnt flush the drive's cache -> use F_FULLFSYNC there
std::expected<void, std::string> SyncFd(int fd);
// needed so creating/renaming files in dir survives a crash
std::expected<void, std::string> SyncDir(const std::filesystem::path &dir);

std::expected<std::string, std::string>
ReadFile(const std::filesystem::path &path);
std::expected<std::string, std::string> ReadAt(int fd, uint64_t offset,
                                               size_t n);

// writes to path.tmp then renames -> path is never half written
std::expected<void, std::string>
WriteFileAtomic(const std::filesystem::path &path, std::string_view data);

} // namespace SmilingDB
