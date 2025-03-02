#include <array>
#include <iterator>
#include <sys/socket.h>
#include "utils/Payload.hpp"
#include "Error.hpp"

namespace utils {
	FilePayload::FilePayload(const std::filesystem::path &filePath) : Payload(), _filePath(filePath) {
		if (!std::filesystem::exists(filePath)) {
			throw FileNotFoundException(_filePath.filename());
		}

		_ifstream.open(filePath, std::ios::binary);

		if (!_ifstream.is_open()) {
			throw std::ios_base::failure("Failed to open " + filePath.string());
		}

		_totalBytes = std::filesystem::file_size(filePath);
	}

	FilePayload::FilePayload(const FilePayload& other) : Payload(other), _filePath(other._filePath) {
		_ifstream.open(_filePath, std::ios::binary);

		if (!_ifstream.is_open()) {
			throw std::ios_base::failure("Failed to open " + _filePath.string());
		}
	}

	FilePayload& FilePayload::operator=(const FilePayload& other) {
		if (this != &other) {
			Payload::operator=(other);
			_filePath = other._filePath;

			_ifstream.open(_filePath, std::ios::binary);

			if (!_ifstream.is_open()) {
				throw std::ios_base::failure("Failed to open " + _filePath.string());
			}
		}

		return *this;
	}

	void FilePayload::send(int fd) {
		if (Payload::_bytesSent >= _totalBytes) {
			return;
		}

		char buffer[4096];

		_ifstream.seekg(Payload::_bytesSent, std::ios::beg);
		const auto bytesRead = _ifstream.read(buffer, sizeof(buffer)).gcount();

		if (_ifstream.bad() || (_ifstream.fail() && !_ifstream.eof())) {
			_ifstream.close();
			throw std::ios_base::failure("Failed to read " + _filePath.string());
		}

		const ssize_t bytesSent = ::send(fd, buffer, bytesRead, MSG_NOSIGNAL);

		if (bytesSent > 0) {
			Payload::_bytesSent += static_cast<std::size_t>(bytesSent);

			if (Payload::_bytesSent >= _totalBytes) {
				_ifstream.close();
			}
		}
	}

	std::string FilePayload::toString() const {
		_ifstream.seekg(0, std::ios::beg);
		return std::string(
			std::istreambuf_iterator<char>(_ifstream),
			std::istreambuf_iterator<char>()
		);
	}

	std::unique_ptr<Payload> FilePayload::clone() const {
		return std::make_unique<FilePayload>(*this);
	}
}
