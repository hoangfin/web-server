#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <iostream>
#include <algorithm>
#include <array>
#include <iterator>

#include "http/index.hpp"
#include "utils/common.hpp"

using std::chrono::steady_clock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;

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

		if (bytesRead > 0) {
			_buffer.reserve(_buffer.size() + bytesRead);
			_buffer.insert(_buffer.end(), buf, buf + bytesRead);
			_lastReceived = steady_clock::now();

			if (_requestHandleStart == TimePoint::min()) {
				_requestHandleStart = steady_clock::now();
			}

			if (_queue.size() > 0) {
				return;
			}

			_processBuffer();

			if (_request.getStatus() == Request::Status::BAD || _request.getStatus() == Request::Status::COMPLETE) {
				_requestHandleStart = TimePoint::min();
				Response res(_clientFd);

				res.onStatusChanged([this](Response::Status status) {
					if (status == Response::Status::PENDING) {
						_responseHandleStart = steady_clock::now();
					} else if (status == Response::Status::READY) {
						_responseHandleStart = TimePoint::min();
					}
				});

				_queue.emplace(std::move(_request), res);
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

		if (_responseDeliveryStart == TimePoint::min()) {
			_responseDeliveryStart = steady_clock::now();
		}

		if (res.send()) {
			_responseDeliveryStart = TimePoint::min();
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
		auto now = std::chrono::steady_clock::now();
		const std::size_t idleTimeDiff = duration_cast<milliseconds>(now - _lastReceived).count();

		if (_requestHandleStart != TimePoint::min() && now >= _requestHandleStart) {
			const std::size_t elapsedTime = duration_cast<milliseconds>(now - _requestHandleStart).count();

			if (elapsedTime >= _serverConfig.msRequestTimeout) {
				std::cout << "Request took too long! Timed out" << std::endl;
				return true;
			}
		}

		if (idleTimeDiff >= _serverConfig.msIdleTimeout) {
			std::cout << "Timed out due to inactivity" << std::endl;
			return true;
		}

		if (_responseHandleStart != TimePoint::min() && now >= _responseHandleStart) {
			const std::size_t elapsedTime = duration_cast<milliseconds>(now - _responseHandleStart).count();

			if (elapsedTime >= _serverConfig.msResponseHandlingTimeout) {
				std::cout << "Processing response timed out" << std::endl;
				return true;
			}
		}

		if (_responseDeliveryStart != TimePoint::min() && now >= _responseDeliveryStart) {
			const std::size_t elapsedTime = duration_cast<milliseconds>(now - _responseDeliveryStart).count();

			if (elapsedTime >= _serverConfig.msResponseDeliveryTimeout) {
				std::cout << "Delivery response timed out" << std::endl;
				return true;
			}
		}

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
