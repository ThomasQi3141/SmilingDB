#pragma once

#include <expected>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace SmilingDB {

// fresh empty dir, deleted (with everything inside) when it goes out of scope
class TempDir {
public:
	TempDir() {
		std::random_device rd;
		path_ =
		    std::filesystem::temp_directory_path() /
		    ("smilingdb_test_" + std::to_string(rd()) + std::to_string(rd()));
		std::filesystem::create_directories(path_);
	}
	~TempDir() {
		std::error_code ec;
		std::filesystem::remove_all(path_, ec);
	}

	TempDir(const TempDir &) = delete;
	TempDir &operator=(const TempDir &) = delete;

	const std::filesystem::path &path() const { return path_; }

private:
	std::filesystem::path path_;
};

// unwraps an expected, throws on error -> gtest reports it as a failure
template <typename T> T Check(std::expected<T, std::string> result) {
	if (!result) {
		throw std::runtime_error(result.error());
	}
	return std::move(*result);
}

inline void Check(std::expected<void, std::string> result) {
	if (!result) {
		throw std::runtime_error(result.error());
	}
}

inline void WriteRaw(const std::filesystem::path &path, std::string_view data,
                     bool append = false) {
	std::ofstream out(path, std::ios::binary |
	                            (append ? std::ios::app : std::ios::trunc));
	out.write(data.data(), static_cast<std::streamsize>(data.size()));
}

} // namespace SmilingDB
