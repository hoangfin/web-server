#include <array>
#include <sys/socket.h>
#include "utils/Payload.hpp"
#include "utils/common.hpp"
#include "Error.hpp"

namespace utils {
	void CgiPayload::send(int fd) {
		if (Payload::_bytesSent >= _totalBytes) {
			return;
		}

		unsigned char* buf = _buffer.data() + Payload::_bytesSent;
		const std::size_t size = _totalBytes - Payload::_bytesSent;

		const ssize_t bytesSent = ::send(fd, buf, size, MSG_NOSIGNAL);

		if (bytesSent > 0) {
			Payload::_bytesSent += static_cast<std::size_t>(bytesSent);
		}
	}

	void CgiPayload::append(const std::uint8_t* data, size_t size) {
		_buffer.insert(_buffer.end(), data, data + size);

		if (_headerFields.empty()) {
			parseHeaderFields(_headerFields, _buffer);
		}

		_totalBytes = _buffer.size();
	}

	std::string CgiPayload::toString() const {
		return std::string(_buffer.begin(), _buffer.end());
	}

	std::unique_ptr<Payload> CgiPayload::clone() const {
		return std::make_unique<CgiPayload>(*this);
	}

	const std::unordered_map<std::string, std::string>& CgiPayload::headerFields() const {
		return _headerFields;
	}
}
