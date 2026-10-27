#pragma once

#include "log_record.h"

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace SmilingDB {

class LogWriter {
public:
	// appends to path (creates it if missing), sync -> fsync after every record
	static std::expected<std::unique_ptr<LogWriter>, std::string>
	Open(const std::filesystem::path &path, bool sync);
	~LogWriter();

	LogWriter(const LogWriter &) = delete;
	LogWriter &operator=(const LogWriter &) = delete;

	std::expected<void, std::string>
	AddRecord(RecordType type, std::string_view key, std::string_view value);

private:
	LogWriter(int fd, bool sync) : fd_(fd), sync_(sync) {}

	int fd_;
	bool sync_;
};

} // namespace SmilingDB
