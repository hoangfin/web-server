#include <regex>
#include <ranges>
#include <sstream>

#include "http/parser.hpp"
#include "utils/common.hpp"

namespace {
	void parseRequestLine(const std::string& requestLine, http::Request& request) {
		std::regex requestLineRegex(R"(^(GET|POST|PUT|DELETE|HEAD|OPTIONS|PATCH|TRACE|CONNECT) (\S+) HTTP\/1\.1$)");
		std::smatch matches;

		if (!std::regex_match(requestLine, matches, requestLineRegex)) {
			throw std::invalid_argument("Malformed or invalid request line");
		}

		std::istringstream istream(requestLine);
		std::string method;
		std::string uri;
		std::string version;

		istream >> method >> uri >> version;

		request
			.setMethod(method)
			.setUri(uri)
			.setVersion(version);
	}

	void parseRequestHeaderFields(const std::string &headerFields, http::Request& request) {
		std::istringstream istream(headerFields);
		std::string line;

		while (std::getline(istream, line) && line != "\r") {
			if (!http::isValidHeaderField(line)) {
				continue;
			}

			std::size_t colonPos = line.find(":");
			std::string name = line.substr(0, colonPos);
			std::string value = utils::trimSpace(line.substr(colonPos + 1));

			if (name == http::stringOf(http::Header::TRANSFER_ENCODING) && value == "chunked" && request.getMethod() == "GET") {
				throw std::invalid_argument("Chunked transfer encoding is not allowed in GET requests");
			}

			if (name == stringOf(http::Header::CONTENT_LENGTH)) {
				if (value.empty() || !std::ranges::all_of(value, ::isdigit)) {
					throw std::invalid_argument("Invalid Content-Length: " + value);
				}

				try {
					request.setContentLength(std::stoull(value));
				} catch(const std::exception& e) {
					throw std::invalid_argument("Invalid Content-Length: " + value);
				}
			}

			request.setHeader(name, value);
		}

		if (!request.getHeader(http::Header::HOST).has_value()) {
			throw std::invalid_argument("No Host found in header request");
		}
	}

	std::size_t parseChunkSize(std::string chunkSizeLine) {
		std::size_t semicolonPos = chunkSizeLine.find(";");

		if (semicolonPos != std::string::npos) {
			chunkSizeLine = chunkSizeLine.substr(0, semicolonPos);
		}

		std::size_t chunkSize;
		std::istringstream stream(chunkSizeLine);

		if (!(stream >> std::hex >> chunkSize)) {
			throw std::invalid_argument("Invalid chunk size");
		}

		return chunkSize;
	}

	void dechunk(std::vector<uint8_t>& buffer, http::Request& request, std::size_t clientMaxBodySize) {
		std::vector<uint8_t> rawData;
		bool isChunkEnd = false;
		auto begin = buffer.begin();
		auto end = buffer.end();
		auto currentPos = begin;

		rawData.reserve(buffer.size());

		while (true) {
			auto delimPos = utils::findDelimiter(currentPos, end, {'\r', '\n'});

			if (delimPos == end || std::distance(delimPos, end) < 2) {
				break;
			}

			std::size_t chunkSize = parseChunkSize(std::string(currentPos, delimPos));
			std::size_t distance = static_cast<std::size_t>(std::distance(delimPos + 2, end));

			if (distance < chunkSize + 2) {
				break;
			}

			currentPos = delimPos + 2;

			if (chunkSize == 0) {
				if (*currentPos == '\r' && *(currentPos + 1) == '\n') {
					isChunkEnd = true;
					currentPos += 2;
					break;
				}

				throw std::invalid_argument(R"(Error: Chunk body did not end with 0\r\n\r\n)");
			}

			rawData.insert(rawData.end(), currentPos, currentPos + chunkSize);
			currentPos += chunkSize + 2;
		}

		if (request.getRawBody().size() + rawData.size() >= clientMaxBodySize) {
			throw std::invalid_argument("Exceeded request max body size");
		}

		request.setRawBody(
			std::make_move_iterator(rawData.begin()),
			std::make_move_iterator(rawData.end()),
			true
		);

		if (isChunkEnd) {
			request.setStatus(http::Request::Status::COMPLETE);
		}

		buffer.erase(begin, currentPos);
	}

	void parseMultipartHeader(const std::string& header, http::MultipartElement& element) {
		std::istringstream istream(header);
		std::string line;
		std::size_t startPos;
		std::size_t endPos;

		std::getline(istream, line);
		std::getline(istream, line);
		startPos = line.find("name=");
		endPos = line.find(";", startPos);
		element.name = utils::trimSpace(line.substr(startPos + 5, endPos - startPos - 5));

		if (element.name.front() == '"' && element.name.back() == '"') {
			if (element.name.size() == 2) {
				element.name = "";
			} else if (element.name.size() > 2) {
				element.name = element.name.substr(1, element.name.size() - 2);
			}
		}

		startPos = line.find("filename=");
		element.fileName = utils::trimSpace(line.substr(startPos + 9));

		if (element.fileName.front() == '"' && element.fileName.back() == '"') {
			if (element.fileName.size() == 2) {
				element.fileName = "";
			} else if (element.fileName.size() > 2) {
				element.fileName = element.fileName.substr(1, element.fileName.size() - 2);
			}
		}

		std::getline(istream, line);
		startPos = line.find(":");
		element.contentType = utils::trimSpace(line.substr(startPos + 1));
	}
}

namespace http {
	Url parseUrl(const std::string& fullUrl) {
		Url result;

		std::regex urlRegex(R"((https?://)?(?:([^:@]+)(?::([^:@]*))?@)?([^:/?#]+)(?::(\d+))?(/[^?#]*)?(?:\?([^#]*))?(?:#(.*))?)");
		std::smatch matches;

		if (!std::regex_match(fullUrl, matches, urlRegex)) {
			throw std::invalid_argument("Invalid URL");
		}

		result.scheme = matches[1].str();
		result.user = matches[2].str();
		result.password = matches[3].str();
		result.host = matches[4].str();
		result.port = matches[5].str();
		result.path = matches[6].str();
		result.query = matches[7].str();
		result.fragment = matches[8].str();
		return result;
	}

	void parseRequestHeader(std::vector<uint8_t>& buffer, Request& request) {
		std::string crlf("\r\n");
		std::string emptyLine("\r\n\r\n");

		auto begin = buffer.begin();
		auto end = buffer.end();
		auto delim = std::search(begin, end, emptyLine.begin(), emptyLine.end());

		if (delim == end) {
			if (buffer.size() >= MAX_REQUEST_HEADER_SIZE) {
				request.setStatus(Request::Status::BAD);
			}
			return;
		}

		auto current = std::search(begin, end, crlf.begin(), crlf.end());

		parseRequestLine(std::string(begin, current), request);
		parseRequestHeaderFields(std::string(current + 2, delim + 4), request);

		Url url = parseUrl(request.getHeader(Header::HOST).value_or("") + request.getUri());

		request.setUrl(url).setStatus(Request::Status::HEADER_COMPLETE);
		buffer.erase(begin, delim + 4);
	}

	void parseRequestBody(std::vector<uint8_t>& buffer, Request& request, std::size_t clientMaxBodySize) {
		// std::cout << "parseRequestBody() called" << std::endl;
		if (request.isChunkEncoding()) {
			return dechunk(buffer, request, clientMaxBodySize);
		}

		std::size_t contentLength = request.getContentLength();

		if (contentLength >= clientMaxBodySize) {
			std::cout << "Exceeded request max body size" << std::endl;
			throw std::invalid_argument("Exceeded request max body size");
		}

		if (buffer.size() < contentLength) {
			return;
		}

		request
			.setRawBody(
				std::make_move_iterator(buffer.begin()),
				std::make_move_iterator(buffer.begin() + contentLength),
				false
			)
			.setStatus(Request::Status::COMPLETE);

		if (request.isMultipart()) {
			std::cout << "request.isMultipart" << std::endl;
			std::string finalBoundary("--" + request.getBoundary() + "--\r\n");
			auto begin = request.getRawBody().begin();
			auto end = request.getRawBody().end();

			if (std::search(begin, end, finalBoundary.begin(), finalBoundary.end()) == end) {
				std::cout << "Could not find finalBoundary" << finalBoundary << std::endl;
				request.setStatus(Request::Status::BAD);
			}
		}
	}

	std::vector<MultipartElement> parseMultipart(const std::vector<uint8_t>& rawMultipart, const std::string& boundary) {
		std::vector<MultipartElement> elements;
		std::string startBoundary("--" + boundary);
		std::string finalBoundary("--" + boundary + "--\r\n");
		std::string emptyLine("\r\n\r\n");

		auto begin = rawMultipart.begin();
		auto end = std::search(begin, rawMultipart.end(), finalBoundary.begin(), finalBoundary.end());
		auto current = begin;

		while (begin != end) {
			MultipartElement element;
			current = std::search(begin, end, emptyLine.begin(), emptyLine.end());
			parseMultipartHeader(std::string(begin, current + 2), element);
			begin = current + 4;
			current = std::search(begin, end, startBoundary.begin(), startBoundary.end());
			element.rawData.insert(element.rawData.end(), begin, current);
			elements.push_back(element);
			begin = current;
		}

		return elements;
	}
}
