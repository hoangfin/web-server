#pragma once

#include "Config.hpp"
#include <vector>
#include <string>
#include <initializer_list>
#include <algorithm>
#include <iterator>
// #include "http/Request.hpp"

namespace utils {
	using LineHandler = std::function<void(const std::string&)>;

	std::string generate_random_string();
	std::string lowerCase(std::string str);
	std::string sanitizePath(const std::string& root, const std::string& path);
	std::string trimSpace(const std::string& str);
    std::string removeComments(const std::string &str);
    std::string trim(const std::string &str);

    int parsePort(const std::string &value);

	bool isValidPath(const std::string& rawPath);
    bool isValidFilePath(const std::string &path);
    bool isValidIP(const std::string &ip);
    bool isValidSize(const std::string &size);
    bool isValidURL(const std::string &url);
    bool parseBool(const std::string &value);

	template <typename T>
	bool isInVector(const T& element, const std::vector<T>& vec) {
		return std::find(vec.begin(), vec.end(), element) != vec.end();
	}

	void closeFDs(const std::vector<int>& serverFds);
	void parseBlock(std::ifstream &file, const std::string &blockType, const LineHandler &lineHandler);
	void parseKeyValue(const std::string &line, const ParserMap &parsers);
    void validateErrorPage(const std::string &code, const std::string &path);
    void validateMethods(const std::vector<std::string> &methods);

	std::size_t convertSizeToBytes(const std::string& sizeStr);
	std::pair<std::string, std::string> splitKeyValue(const std::string& line);

	template <typename Iterator>
	Iterator findDelimiter(
		const Iterator begin,
		const Iterator end,
		std::initializer_list<typename std::iterator_traits<Iterator>::value_type> delimiter
	) {
		return std::search(begin, end, delimiter.begin(), delimiter.end());
	}

	std::filesystem::path computeFilePath(const Location& loc, const std::string& requestPath);
	std::string getFileExtension(const std::string& filePath);
	bool hasRequiredMethods(const std::vector<std::string>& methods);

	void parseHeaderFields(
		std::unordered_map<std::string, std::string>& headerFields,
		std::vector<std::uint8_t>& buffer
	);
	// FOR TESTING
	// void printRequest(const http::Request& request);
	// void printServerConfig(const ServerConfig& server);
	// void printConfig(const Config& config);

}
