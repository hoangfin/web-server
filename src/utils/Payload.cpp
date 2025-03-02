#include <typeinfo>
#include "utils/Payload.hpp"

namespace utils {
	void Payload::append(const std::uint8_t* data, size_t size) {
		(void)data;
		(void)size;
		std::string className(typeid(*this).name());
		throw std::runtime_error(className + " does not support method append(const std::uint8_t*, size_t)");
	}

	bool Payload::isSent() const {
		return (_bytesSent >= _totalBytes);
	}

	std::size_t Payload::size() const {
		return _totalBytes;
	}
}
