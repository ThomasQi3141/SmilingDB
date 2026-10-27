#include "log_writer.h"
#include "file_util.h"
#include "log_record.h"

#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unistd.h>

namespace SmilingDB {

std::expected<std::unique_ptr<LogWriter>, std::string>
LogWriter::Open(const std::filesystem::path &path, bool sync) {
	int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fd < 0) {
		return std::unexpected(ErrnoMessage("open " + path.string()));
	}
	if (sync) {
		if (std::expected<void, std::string> r = SyncDir(path.parent_path());
		    !r) {
			::close(fd);
			return std::unexpected(r.error());
		}
	}
	return std::unique_ptr<LogWriter>(new LogWriter(fd, sync));
}

LogWriter::~LogWriter() { ::close(fd_); }

std::expected<void, std::string> LogWriter::AddRecord(RecordType type,
                                                      std::string_view key,
                                                      std::string_view value) {
	std::expected<void, std::string> result =
	    WriteAll(fd_, EncodeRecord(type, key, value));
	if (!result || !sync_) {
		return result;
	}
	return SyncFd(fd_);
}

} // namespace SmilingDB
