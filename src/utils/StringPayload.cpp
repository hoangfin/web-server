#include <array>
#include <sys/socket.h>
#include "utils/Payload.hpp"
#include "Error.hpp"

namespace utils {
	StringPayload::StringPayload(const std::string& message) : Payload(), _message(message) {
		_totalBytes = message.size();
	}

	void StringPayload::send(int fd) {
		if (Payload::_bytesSent >= _totalBytes) {
			return;
		}

		const char* buf = _message.data() + Payload::_bytesSent;
		const std::size_t size = _totalBytes - Payload::_bytesSent;

		const ssize_t bytesSent = ::send(fd, buf, size, MSG_NOSIGNAL);

		if (bytesSent > 0) {
			Payload::_bytesSent += static_cast<std::size_t>(bytesSent);
		}
	}

	void StringPayload::append(const std::uint8_t* data, size_t size) {
		_message.append(reinterpret_cast<const char*>(data), size);
		_totalBytes = _message.size();
	}

	std::string StringPayload::toString() const {
		return _message;
	}

	void StringPayload::setMessage(const std::string& message) {
		_message = message;
		_totalBytes = _message.size();
	}

	std::unique_ptr<Payload> StringPayload::clone() const {
		return std::make_unique<StringPayload>(*this);
	}
}
