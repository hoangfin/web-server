#include <sstream>
#include <cstdint>
#include <sys/socket.h>
#include "http/Response.hpp"
#include "Error.hpp"
#include "utils/index.hpp"
#include "http/utils.hpp"

namespace http {
	Response::Response(int clientSocket) : _clientSocket(clientSocket) , _header(utils::StringPayload("")) {}

	Response::Response(const Response& other)
		: _clientSocket(other._clientSocket)
		, _status(other._status)
		, _statusCode(other._statusCode)
		, _headerFields(other._headerFields)
		, _header(other._header)
		, _body(other._body ? other._body->clone() : nullptr) {
	}

	Response& Response::operator=(const Response& other) {
		if (this != &other) {
			_clientSocket = other._clientSocket;
			_status = other._status;
			_statusCode = other._statusCode;
			_headerFields = other._headerFields;
			_header = other._header;
			_body = other._body ? other._body->clone() : nullptr;
		}

		return *this;
	}

	bool Response::send() {
		if (!_header.isSent()) {
			_header.send(_clientSocket);
			return false;
		}

		if (_body == nullptr) {
			return true;
		}

		_body->send(_clientSocket);

		if (_body->isSent()) {
			return true;
		}

		return false;
	}

	void Response::build() {
		std::ostringstream ostream;

		ostream
			<< "HTTP/1.1 "
			<< static_cast<std::uint16_t>(_statusCode) << " "
			<< stringOf(_statusCode) << "\r\n";

		for (const auto& [name, value] : _headerFields) {
			ostream << name << ": " << value << "\r\n";
		}

		ostream << "\r\n";
		_header.setMessage(ostream.str());
		setStatus(Response::Status::READY);
	}

	void Response::onStatusChanged(std::function<void(Response::Status status)> handler) {
		_handlers.push_back(handler);
	}

	int Response::getClientSocket() const {
		return _clientSocket;
	}

	Response::Status Response::getStatus() const {
		return _status;
	}

	StatusCode Response::getStatusCode() const {
		return _statusCode;
	}

	const utils::StringPayload& Response::getHeader() const {
		return _header;
	}

	const std::unique_ptr<utils::Payload>& Response::getBody() const {
		return _body;
	}

	Response& Response::clear() {
		_statusCode = StatusCode::NONE_0;
		_headerFields.clear();
		_header.setMessage("");
		_body.reset();
		_handlers.clear();
		return *this;
	}

	Response& Response::setStatus(const Status status) {
		_status = status;

		for (const auto& handler : _handlers) {
			handler(_status);
		}

		return *this;
	}

	Response& Response::setStatusCode(const StatusCode statusCode) {
		_statusCode = statusCode;
		return *this;
	}

	Response& Response::setHeader(Header header, const std::string& value) {
		_headerFields[stringOf(header)] = value;
		return *this;
	}

	Response& Response::setHeader(const std::string& headerName, const std::string& headerValue) {
		_headerFields[headerName] = headerValue;
		return *this;
	}

	Response& Response::setBody(std::unique_ptr<utils::Payload> body) {
		_body = std::move(body);
		return *this;
	}

	Response& Response::appendBody(const std::uint8_t* data, size_t size) {
		_body->append(data, size);
		return *this;
	}

	void Response::setText(StatusCode statusCode, const std::string& text) {
		setStatusCode(statusCode);
		setBody(std::make_unique<utils::StringPayload>(text));
		setHeader(Header::CONTENT_TYPE, getMimeType("txt"));
		setHeader(Header::CONTENT_LENGTH, std::to_string(_body->size()));
		build();
	}

	void Response::setFile(StatusCode statusCode, const std::filesystem::path &filePath) {
		std::string ext = filePath.extension().string().erase(0, 1); // Get file extension without '.'

		setStatusCode(statusCode);
		setBody(std::make_unique<utils::FilePayload>(filePath));
		setHeader(Header::CONTENT_TYPE, getMimeType(ext));
		setHeader(Header::CONTENT_LENGTH, std::to_string(_body->size()));
		// setHeader(Header::CACHE_CONTROL, "public, max-age=86400");	// For production mode
		setHeader(Header::CACHE_CONTROL, "no-store"); 				// For test mode
		build();
	}
}
