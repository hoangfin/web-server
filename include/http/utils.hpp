#pragma once

#include <string>
#include <array>
#include <unordered_map>
#include "constants.hpp"
#include "utils/common.hpp"

namespace http {
	std::string getMimeType(const std::string &extension);
	std::string getExtensionFromMimeType(const std::string& mime);
	std::string stringOf(Header header);
	std::string stringOf(StatusCode code);

	bool hasHeaderName(const std::string &headerName);
	bool isValidHeaderField(const std::string &headerField);

	template <typename Iterator>
	std::unordered_map<std::string, std::string> extractHeaderFields(Iterator begin, Iterator end) {
		std::unordered_map<std::string, std::string> headerFields;
		std::string emptyLine("\r\n\r\n");
		std::cout << "rawData" << std::string(begin, end) << std::endl;
		auto it = std::search(begin, end, emptyLine.begin(), emptyLine.end());

		if (it != end) {
			std::string rawHeader(begin, it + 4);
			// std::cout << "rawHeader" << rawHeader << std::endl;
			std::istringstream istream(rawHeader);
			std::string line;

			while (std::getline(istream, line) && line != "\r") {
				std::size_t colonPos = line.find(":");
				std::string headerName = line.substr(0, colonPos);
				std::string headerValue = utils::trimSpace(line.substr(colonPos + 1));
				headerFields[headerName] = headerValue;
			}
		}

		return headerFields;
	}
}
