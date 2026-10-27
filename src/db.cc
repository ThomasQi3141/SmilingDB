#include "db.h"
#include "file_util.h"
#include "log_reader.h"
#include "log_record.h"
#include "log_writer.h"
#include "memtable.h"
#include "table_builder.h"
#include "table_reader.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace SmilingDB {

namespace {

std::filesystem::path LogFileName(const std::filesystem::path &dir,
                                  uint64_t number) {
	return dir / std::format("{:06}.log", number);
}

std::filesystem::path TableFileName(const std::filesystem::path &dir,
                                    uint64_t number) {
	return dir / std::format("{:06}.sst", number);
}

std::optional<uint64_t> ParseFileNumber(const std::filesystem::path &path) {
	std::string stem = path.stem().string();
	if (stem.empty()) {
		return std::nullopt;
	}
	uint64_t number = 0;
	auto [end, ec] =
	    std::from_chars(stem.data(), stem.data() + stem.size(), number);
	if (ec != std::errc{} || end != stem.data() + stem.size()) {
		return std::nullopt;
	}
	return number;
}

} // namespace

std::expected<std::unique_ptr<DB>, std::string>
DB::Open(const std::filesystem::path &dir, Options options) {
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	if (ec) {
		return std::unexpected("create " + dir.string() + ": " + ec.message());
	}
	std::unique_ptr<DB> db(new DB(dir, options));

	std::vector<uint64_t> logs;
	std::vector<uint64_t> tables;
	std::vector<std::filesystem::path> leftovers;
	for (auto it = std::filesystem::directory_iterator(dir, ec);
	     !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
		std::filesystem::path path = it->path();
		// .tmp = crashed mid flush, the real file was never renamed into place
		if (path.extension() == ".tmp") {
			leftovers.push_back(path);
			continue;
		}
		std::optional<uint64_t> number = ParseFileNumber(path);
		if (!number) {
			continue;
		}
		if (path.extension() == ".log") {
			logs.push_back(*number);
		} else if (path.extension() == ".sst") {
			tables.push_back(*number);
		} else {
			continue;
		}
		db->next_file_number_ = std::max(db->next_file_number_, *number + 1);
	}
	if (ec) {
		return std::unexpected("list " + dir.string() + ": " + ec.message());
	}
	for (const std::filesystem::path &path : leftovers) {
		std::filesystem::remove(path, ec);
	}
	std::ranges::sort(logs);
	std::ranges::sort(tables);

	for (uint64_t number : tables) {
		auto table = TableReader::Open(TableFileName(dir, number));
		if (!table) {
			return std::unexpected(table.error());
		}
		db->tables_.push_back(std::move(*table));
	}
	for (uint64_t number : logs) {
		if (auto r = ReplayLog(LogFileName(dir, number), *db->mem_); !r) {
			return std::unexpected(r.error());
		}
	}
	// flush whatever the logs had into a table, then delete the logs -> never
	// have to append after a torn last record
	if (!db->mem_->Empty()) {
		if (auto r = db->WriteTable(*db->mem_); !r) {
			return std::unexpected(r.error());
		}
		db->mem_ = std::make_unique<MemTable>();
	}
	for (uint64_t number : logs) {
		std::filesystem::remove(LogFileName(dir, number), ec);
		if (ec) {
			return std::unexpected("remove log: " + ec.message());
		}
	}
	if (auto r = db->NewLog(); !r) {
		return std::unexpected(r.error());
	}
	return db;
}

// write to the log FIRST, then the memtable
std::expected<void, std::string> DB::Put(std::string key, std::string value) {
	if (auto r = log_->AddRecord(RecordType::Put, key, value); !r) {
		return r;
	}
	mem_->Put(std::move(key), std::move(value));
	return MaybeFlush();
}

std::expected<void, std::string> DB::Remove(std::string key) {
	if (auto r = log_->AddRecord(RecordType::Delete, key, ""); !r) {
		return r;
	}
	mem_->Remove(std::move(key));
	return MaybeFlush();
}

// memtable first, then tables newest -> oldest, first Found OR Deleted wins
std::expected<std::optional<std::string>, std::string>
DB::Get(std::string_view key) const {
	std::string value;
	switch (mem_->Get(key, value)) {
	case LookupResult::Found:
		return value;
	case LookupResult::Deleted:
		return std::nullopt;
	case LookupResult::NotFound:
		break;
	}
	for (auto it = tables_.rbegin(); it != tables_.rend(); ++it) {
		auto result = (*it)->Get(key, value);
		if (!result) {
			return std::unexpected(result.error());
		}
		switch (*result) {
		case LookupResult::Found:
			return value;
		case LookupResult::Deleted:
			return std::nullopt;
		case LookupResult::NotFound:
			break;
		}
	}
	return std::nullopt;
}

std::expected<void, std::string> DB::MaybeFlush() {
	if (mem_->ApproximateMemoryUsage() < options_.write_buffer_size) {
		return {};
	}
	if (auto r = WriteTable(*mem_); !r) {
		return r;
	}
	uint64_t old_log = log_number_;
	if (auto r = NewLog(); !r) {
		return r;
	}
	mem_ = std::make_unique<MemTable>();
	// only safe once the table is synced, the old log was the only other copy
	std::error_code ec;
	std::filesystem::remove(LogFileName(dir_, old_log), ec);
	if (ec) {
		return std::unexpected("remove log: " + ec.message());
	}
	return {};
}

std::expected<void, std::string> DB::WriteTable(const MemTable &mem) {
	uint64_t number = next_file_number_++;
	TableBuilder builder(options_.block_size);
	// tombstones get written too, otherwise an older table's value comes back
	for (auto [key, entry] : mem) {
		builder.Add(key, entry);
	}
	std::filesystem::path path = TableFileName(dir_, number);
	if (auto r = WriteFileAtomic(path, builder.Finish()); !r) {
		return r;
	}
	auto table = TableReader::Open(path);
	if (!table) {
		return std::unexpected(table.error());
	}
	tables_.push_back(std::move(*table));
	return {};
}

std::expected<void, std::string> DB::NewLog() {
	uint64_t number = next_file_number_++;
	auto log = LogWriter::Open(LogFileName(dir_, number), options_.sync);
	if (!log) {
		return std::unexpected(log.error());
	}
	log_ = std::move(*log);
	log_number_ = number;
	return {};
}

} // namespace SmilingDB
