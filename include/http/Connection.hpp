#pragma once

#include <vector>
#include <queue>
#include <utility>
#include <chrono>
#include <functional>
#include "Request.hpp"
#include "Response.hpp"
#include "Config.hpp"

namespace http {
	class Connection {
		public:
			Connection(int clientSocket, const ServerConfig& serverConfig);
			Connection(const Connection&) = default;
			~Connection() = default;

			Connection& operator=(const Connection&) = delete;

			void read();
			bool sendResponse();
			void close();

			bool isClosed() const;
			bool isTimedOut() const;

			Request* getRequest();
			Response* getResponse();

			int getClientFd() const;

		private:
			using TimePoint = std::chrono::steady_clock::time_point;

			int _clientFd;
			const ServerConfig& _serverConfig;
			Request _request { Request::Status::PENDING };
			std::vector<std::uint8_t> _buffer;
			std::queue<std::pair<Request, Response>> _queue;
			TimePoint _lastReceived;
			TimePoint _requestHandleStart { TimePoint::min() };
			TimePoint _responseHandleStart { TimePoint::min() };
			TimePoint _responseDeliveryStart { TimePoint::min() };

			void _processBuffer();
	};
}
