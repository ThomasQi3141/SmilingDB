#include "file_util.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace SmilingDB {

std::string ErrnoMessage(std::string_view what) {
	return std::string(what) + ": " + std::strerror(errno);
}

std::expected<void, std::string> WriteAll(int fd, std::string_view data) {
	while (!data.empty()) {
		ssize_t n = ::write(fd, data.data(), data.size());
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			return std::unexpected(ErrnoMessage("write"));
		}
		data.remove_prefix(static_cast<size_t>(n));
	}
	return {};
}

std::expected<void, std::string> SyncFd(int fd) {
#ifdef F_FULLFSYNC
	if (::fcntl(fd, F_FULLFSYNC) == 0) {
		return {};
	}
	// not every filesystem supports it, fallback to fsync
#endif
	if (::fsync(fd) != 0) {
		return std::unexpected(ErrnoMessage("fsync"));
	}
	return {};
}

std::expected<void, std::string> SyncDir(const std::filesystem::path &dir) {
	std::filesystem::path target = dir.empty() ? "." : dir;
	int fd = ::open(target.c_str(), O_RDONLY);
	if (fd < 0) {
		return std::unexpected(ErrnoMessage("open " + target.string()));
	}
	std::expected<void, std::string> result = SyncFd(fd);
	::close(fd);
	return result;
}

std::expected<std::string, std::string>
ReadFile(const std::filesystem::path &path) {
	int fd = ::open(path.c_str(), O_RDONLY);
	if (fd < 0) {
		return std::unexpected(ErrnoMessage("open " + path.string()));
	}
	struct stat st;
	if (::fstat(fd, &st) != 0) {
		std::string error = ErrnoMessage("stat " + path.string());
		::close(fd);
		return std::unexpected(error);
	}
	std::expected<std::string, std::string> data =
	    ReadAt(fd, 0, static_cast<size_t>(st.st_size));
	::close(fd);
	return data;
}

std::expected<std::string, std::string> ReadAt(int fd, uint64_t offset,
                                               size_t n) {
	std::string out(n, '\0');
	size_t done = 0;
	while (done < n) {
		ssize_t r = ::pread(fd, out.data() + done, n - done,
		                    static_cast<off_t>(offset + done));
		if (r < 0) {
			if (errno == EINTR) {
				continue;
			}
			return std::unexpected(ErrnoMessage("pread"));
		}
		if (r == 0) {
			return std::unexpected("pread: unexpected end of file");
		}
		done += static_cast<size_t>(r);
	}
	return out;
}

std::expected<void, std::string>
WriteFileAtomic(const std::filesystem::path &path, std::string_view data) {
	std::filesystem::path tmp = path;
	tmp += ".tmp";
	int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		return std::unexpected(ErrnoMessage("open " + tmp.string()));
	}
	std::expected<void, std::string> result = WriteAll(fd, data);
	if (result) {
		result = SyncFd(fd);
	}
	::close(fd);
	if (!result) {
		std::error_code ec;
		std::filesystem::remove(tmp, ec);
		return result;
	}
	if (::rename(tmp.c_str(), path.c_str()) != 0) {
		return std::unexpected(ErrnoMessage("rename " + tmp.string()));
	}
	return SyncDir(path.parent_path());
}

} // namespace SmilingDB
