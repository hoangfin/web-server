#include <sstream>
#include <cstring>
#include <array>
#include <algorithm>
#include <sys/socket.h>
#include "utils/index.hpp"
#include "http/Request.hpp"
#include "http/utils.hpp"

namespace http {
	Request::Request(Status status) : _status(status) {}

	void Request::clear() {
		_method.clear();
		_url = Url();
		_version.clear();
		_headerFields.clear();
		_rawBody.clear();
		_status = Request::Status::PENDING;
	}

	bool Request::isChunkEncoding() const {
		return (getHeader(Header::TRANSFER_ENCODING).value_or("") == "chunked");
	}

	bool Request::isMultipart() const {
		return (getHeader(Header::CONTENT_TYPE).value_or("").starts_with("multipart/form-data"));
	}

	std::vector<std::string> Request::getCgiEnvp() const {
		std::vector<std::string> envp;

		envp.push_back("REQUEST_METHOD=" + _method);
		envp.push_back("QUERY_STRING=" + _url.query);
		envp.push_back("CONTENT_LENGTH=" + getHeader(Header::CONTENT_LENGTH).value_or(""));
		envp.push_back("CONTENT_TYPE=" + getHeader(Header::CONTENT_TYPE).value_or(""));
		envp.push_back("SCRIPT_NAME=" + _url.path);
		// envp.push_back("REMOTE_ADDR=" + The IP address of the client making the request.
		envp.push_back("SERVER_NAME=" + _url.host);
		envp.push_back("SERVER_PORT=" + _url.port);
		envp.push_back("SERVER_PROTOCOL=" + _version);
		return envp;
	}

	const std::string& Request::getMethod() const {
		return _method;
	}

	const std::string& Request::getUri() const {
		return _uri;
	}

	const Url& Request::getUrl() const {
		return _url;
	}

	const std::string& Request::getVersion() const {
		return _version;
	}

	std::string Request::getBoundary() const {
		std::string contentType = getHeader(Header::CONTENT_TYPE).value_or("");
		std::string lowercaseContentType = utils::lowerCase(contentType);
		std::size_t pos = lowercaseContentType.find("boundary=");

		if (pos == std::string::npos) {
			return "";
		}

		std::string boundary = contentType.substr(pos + 9);

		if (boundary.size() > 2 && boundary.front() == '"' && boundary.back() == '"') {
			boundary = boundary.substr(1, boundary.size() - 2);
		}

		return boundary;
	}

	std::size_t Request::getContentLength() const {
		return _contentLength;
	}

	std::optional<std::string> Request::getHeader(Header header) const {
		auto it = _headerFields.find(stringOf(header));

		if (it == _headerFields.end()) {
			return std::nullopt;
		}

		return it->second;
	}


	const std::vector<std::uint8_t>& Request::getRawBody() const {
		return _rawBody;
	}

	Request::Status Request::getStatus() const {
		return _status;
	}

	Request& Request::setRawBody(
		std::vector<uint8_t>::const_iterator begin,
		std::vector<uint8_t>::const_iterator end,
		bool append
	) {
		if (append) {
			_rawBody.insert(_rawBody.end(), begin, end);
		} else {
			_rawBody.assign(begin, end);
		}

		return *this;
	}

	Request& Request::setRawBody(
		std::move_iterator<std::vector<uint8_t>::iterator> begin,
		std::move_iterator<std::vector<uint8_t>::iterator> end,
		bool append
	) noexcept {
		if (append) {
			_rawBody.insert(_rawBody.end(), begin, end);
		} else {
			_rawBody.assign(begin, end);
		}

		return *this;
	}

	Request& Request::setContentLength(std::size_t bytes) {
		_contentLength = bytes;
		return *this;
	}

	Request& Request::setHeader(const std::string& name, const std::string& value) {
		_headerFields[name] = value;
		return *this;
	}

	Request& Request::setHeader(Header header, const std::string& value) {
		_headerFields[stringOf(header)] = value;
		return *this;
	}

	Request& Request::setMethod(const std::string& method) {
		_method = method;
		return *this;
	}

	Request& Request::setUri(const std::string& uri) {
		_uri = uri;
		return *this;
	}

	Request& Request::setStatus(Request::Status status) {
		_status = status;
		return *this;
	}

	Request& Request::setUrl(const Url& url) {
		_url = url;
		return *this;
	}

	Request& Request::setVersion(const std::string& version) {
		_version = version;
		return *this;
	}
}
