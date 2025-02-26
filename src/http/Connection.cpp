#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <algorithm>
#include <array>
#include <iterator>

#include "http/Connection.hpp"
#include "http/parser.hpp"
#include "http/utils.hpp"
#include "utils/common.hpp"

namespace http {
	Connection::Connection(int clientSocket, const ServerConfig& serverConfig)
		: _clientFd(clientSocket)
		, _serverConfig(serverConfig) {
	}

	void Connection::read() {
		if (isClosed()) {
			return;
		}

		unsigned char buf[4096];
		ssize_t bytesRead = recv(_clientFd, buf, sizeof(buf), MSG_NOSIGNAL);

		if (bytesRead == 0) {
			this->close();
			return;
		}

		if (bytesRead > 0) {
			_buffer.reserve(_buffer.size() + bytesRead);
			_buffer.insert(_buffer.end(), buf, buf + bytesRead);
			_lastReceived = std::chrono::steady_clock::now();

			if (_queue.size() > 0) {
				return;
			}

			_processBuffer();

			if (_request.getStatus() == Request::Status::BAD || _request.getStatus() == Request::Status::COMPLETE) {
				_queue.emplace(std::move(_request), Response(_clientFd));
				_request.clear();
			}
		}
	}

	bool Connection::sendResponse() {
		if (isClosed() || _queue.empty()) {
			return false;
		}

		auto& [req, res] = _queue.front();

		if (res.getStatus() != Response::Status::READY) {
			return false;
		}

		if (res.send()) {
			const StatusCode code = res.getStatusCode();
			auto connectionHeader = req.getHeader(Header::CONNECTION);
			_queue.pop();

			if (
				connectionHeader.value_or("") == "close"
				|| code == StatusCode::BAD_REQUEST_400
				|| code == StatusCode::REQUEST_TIMEOUT_408
				|| code == StatusCode::INTERNAL_SERVER_ERROR_500
				|| code == StatusCode::SERVICE_UNAVAILABLE_503
				|| code == StatusCode::GATEWAY_TIMEOUT_504
			) {
				this->close();
			}

			return true;
		}

		return false;
	}

	void Connection::close() {
		if (_clientFd == -1) {
			return;
		}

		if (::close(_clientFd) < 0) {
			std::cerr << "Failed to close socket " << _clientFd << ": " << strerror(errno) << std::endl;
		}

		_clientFd = -1;
	}

	bool Connection::isClosed() const {
		return (_clientFd == -1);
	}

	bool Connection::isTimedOut() const {
		// auto elapsedTime = std::chrono::steady_clock::now() - _lastReceived;

		// if (elapsedTime > std::chrono::milliseconds(_serverConfig.timeoutIdle)) {
		// 	return true;
		// }

		return false;
	}

	Request* Connection::getRequest() {
		if (_queue.size() == 0) {
			return nullptr;
		}

		auto& pair = _queue.front();
		return &pair.first;
	}

	Response* Connection::getResponse() {
		if (_queue.size() == 0) {
			return nullptr;
		}

		auto& pair = _queue.front();
		return &pair.second;
	}

	int Connection::getClientFd() const {
		return _clientFd;
	}

	void Connection::_processBuffer() {
		using enum Request::Status;

		try {
			if (_request.getStatus() == PENDING) {
				parseRequestHeader(_buffer, _request);
			}

			if (_request.getStatus() == HEADER_COMPLETE) {
				if (_request.getMethod() == "GET" || _request.getMethod() == "DELETE") {
					_request.setStatus(COMPLETE);
					return;
				}

				if (_request.getMethod() == "POST") {
					parseRequestBody(_buffer, _request, _serverConfig.clientMaxBodySize);
				}
			}
		} catch (const std::invalid_argument &e) {
			_request.setStatus(BAD);
		}
	}
}
