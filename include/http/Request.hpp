#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "data_types.hpp"
#include "constants.hpp"
#include "utils.hpp"

namespace http {
	class Request {
		public:
			enum class Status : uint8_t {
				PENDING,			// The request is not yet processed.
				HEADER_COMPLETE,	// The request has only headers processed.
				BAD,				// The request is malformed or invalid.
				COMPLETE			// The request is fully processed.
			};

			Request() = default;
			explicit Request(Status status);
			Request(const Request&) = default;
			Request(Request&&) noexcept = default;
			~Request() = default;

			Request& operator=(const Request&) = default;
			Request& operator=(Request&&) noexcept = default;

			void clear();

			bool isChunkEncoding() const;
			bool isMultipart() const;

			std::vector<std::string> getCgiEnvp() const;

			const std::string& getMethod() const;
			const std::string& getUri() const;
			const Url& getUrl() const;
			const std::string& getVersion() const;
			std::string getBoundary() const;
			std::size_t getContentLength() const;
			std::optional<std::string> getHeader(Header header) const;
			const std::vector<std::uint8_t>& getRawBody() const;
			Request::Status getStatus() const;

			Request& setRawBody(
				std::vector<uint8_t>::const_iterator begin,
				std::vector<uint8_t>::const_iterator end,
				bool append
			);

			Request& setRawBody(
				std::move_iterator<std::vector<uint8_t>::iterator> begin,
				std::move_iterator<std::vector<uint8_t>::iterator> end,
				bool append
			) noexcept;

			Request& setContentLength(std::size_t bytes);
			Request& setHeader(const std::string& name, const std::string& value);
			Request& setHeader(Header header, const std::string& value);
			Request& setMethod(const std::string& method);
			Request& setUri(const std::string& uri);
			Request& setStatus(Request::Status status);
			Request& setUrl(const Url& url);
			Request& setVersion(const std::string& version);

		private:
			std::string _method;
			std::string _uri;
			std::string _version;
			Url _url;
			std::unordered_map<std::string, std::string> _headerFields;
			std::size_t _contentLength { 0 };
			std::vector<std::uint8_t> _rawBody;
			Request::Status _status { Request::Status::PENDING };
	};
}
