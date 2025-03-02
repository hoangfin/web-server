#include <algorithm> // std::find
#include <cctype> // std::tolower
#include <chrono>
#include <fcntl.h> // fcntl
#include <filesystem> // std::filesystem
#include <iostream> // std::cout, std::endl
#include <random>
#include <regex> // std::regex, std::regex_match
#include <sstream> // std::istringstream
#include <stdexcept> // std::invalid_argument, std::out_of_range
#include <unistd.h> // close

#include "Config.hpp"
#include "Error.hpp"
#include "http/Request.hpp"
#include "http/Response.hpp"
#include "utils/common.hpp"

using std::string;
using std::vector;
using std::cout;
using std::endl;
namespace fs = std::filesystem;

namespace utils {
	string generate_random_string() {
		auto now = std::chrono::system_clock::now();
		auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

		// Generate a random number
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<int> dist(10000, 99999);

		// Combine timestamp and random number for uniqueness
		return std::to_string(now_ms) + "_" + std::to_string(dist(gen));
	}

	bool parseBool(const string &value) {
		if (value == "on") {
			return true;
		}
		if (value == "off") {
			return false;
		}
		THROW_CONFIG_ERROR(EINVAL, "Invalid boolean value");
	}

	int parsePort(const string &value) {
		std::regex port_regex("^[0-9]+$");
		if (!std::regex_match(value, port_regex)) {
			throw ConfigError(EINVAL, "Invalid port number");
		}
		int port = std::stoi(value);
		if (port <= 0 || port > 65535) {
			THROW_CONFIG_ERROR(ERANGE, "Port number out of range");
		}
		return port;
	}

	void validateMethods(const vector<string> &methods) {
		vector<string> validMethods = {"GET", "POST", "DELETE"};
		for (const auto &method : methods) {
			if (std::find(validMethods.begin(), validMethods.end(), method) == validMethods.end()) {
				THROW_CONFIG_ERROR(EINVAL, "Invalid method");
			}
		}
	}

	bool isValidIP(const string &ip) {
		std::regex ipPattern(
			R"((\d{1,3}\.){3}\d{1,3})");
		return std::regex_match(ip, ipPattern);
	}

	bool isValidFilePath(const string &path) {
		return fs::exists(path);
	}

	bool isValidURL(const string& url) {
		const std::regex fullUrlRegex(R"(^https?://[^\s/$.?#].[^\s]*$)");
		const std::regex relativePathRegex(R"(^/[^ ]*$)");
		return std::regex_match(url, fullUrlRegex) || std::regex_match(url, relativePathRegex);
	}

	bool isValidSize(const string &size) {
		std::regex sizePattern(
			R"(\d+([KMGkmg])?)");
		return std::regex_match(size, sizePattern);
	}

	void validateErrorPage(const string &code, const string &path) {
		std::regex code_regex("^[1-5][0-9][0-9]$");
		if (!std::regex_match(code, code_regex)) {
			THROW_CONFIG_ERROR(EINVAL, "Invalid error code");
		}
		if (!isValidFilePath(path)) {
			THROW_CONFIG_ERROR(EINVAL, "Invalid error page path");
		}
	}

	string trim(const string& str) {
		size_t first = str.find_first_not_of(" \t;");
		size_t last = str.find_last_not_of(" \t;");
		return (first == string::npos) ? "" : str.substr(first, (last - first + 1));
	}

	string trimSpace(const string& str) {
		// Find the first non-space character
		auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) {
			return std::isspace(ch);
		});

		// Find the last non-space character
		auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
			return std::isspace(ch);
		}).base();

		// Create a substring without leading and trailing spaces
		return (start < end) ? string(start, end) : string();
	}

	string removeComments(const string& str) {
		size_t commentPos = str.find('#');
		if (commentPos != string::npos) {
			return str.substr(0, commentPos);
		}
		return str;
	}

	string lowerCase(string str) {
		std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
			return std::tolower(c);
		});
		return str;
	}
	void closeFDs(const vector<int>& serverFds) {
    for (int fd : serverFds)
        close(fd);
	}

	std::size_t convertSizeToBytes(const string& sizeStr) {
		std::size_t size = std::stoul(sizeStr);

		// handle lowercase and uppercase K, M, G
		switch (sizeStr.back()) {
			case 'K':
			case 'k':
				size *= 1024;
				break;
			case 'M':
			case 'm':
				size *= 1024 * 1024;
				break;
			case 'G':
			case 'g':
				size *= 1024 * 1024 * 1024;
				break;
			default:
				break;
		}
		return size;
	}

	// Test valid case: root = /Users/username/Webserv/config, path = /static/index.html
	// Expected: /Users/username/Webserv/config/static/index.html
	// Test invalid case: root = /Users/username/Webserv/config, path = /../index.html -> fullPath = /Users/username/Webserv/index.html
	// Test invalid case: root = /Users/username/Webserv/config, path = /static/../index.html -> fullPath = /Users/username/Webserv/index.html
	// Test invalid case: root = /Users/username/Webserv/config, path = /static/../../index.html -> fullPath = /Users/username/index.html
	// Test invalid case: root = /Users/username/Webserv/config, path = /static/../../../index.html -> fullPath = /Users/index.html
	// Expected: ConfigError exception
	string sanitizePath(const string& root, const string& path) {
		fs::path fullPath;
		if (path.front() == '/') {
			fullPath = fs::canonical(root + path);
		} else {
			fullPath = fs::canonical(root + "/" + path);
		}
		fs::path rootPath = fs::canonical(root);
		if (fullPath.string().find(rootPath.string()) != 0) {
			THROW_CONFIG_ERROR(EINVAL, "Path traversal attempt detected");
		}
		return fullPath.string();
	}

	std::pair<string, string> splitKeyValue(const string& line) {
		std::istringstream iss(line);
		string key, value;
		if (iss >> key) {
			std::getline(iss, value);
			value = trim(removeComments(value));

			// Special handling for `location` directives
			if (key == "location") {
				// Remove trailing '{' if present
				size_t bracePos = value.find('{');
				if (bracePos != string::npos) {
					value = value.substr(0, bracePos);
				}
				value = trim(value); // Trim again after removing `{`
			}
		}
		return {key, value};
	}

	void parseKeyValue(const string &line, const ParserMap &parsers) {
		auto [key, value] = utils::splitKeyValue(line);
		//cout << YELLOW "Key: " RESET << key << YELLOW " Value: " RESET << value << endl;

		if (key.empty() || value.empty()) {
			THROW_CONFIG_ERROR(EINVAL, "Invalid directive in configuration block");
		}

		auto it = parsers.find(key);
		if (it != parsers.end()) {
			it->second(value);
		} else {
			THROW_CONFIG_ERROR(EINVAL, "Invalid directive in configuration block");
		}
	}

	void parseBlock(std::ifstream &file, const string &blockType, const LineHandler &lineHandler) {
		string line;
		while (std::getline(file, line)) {
			line = trim(removeComments(line));
			if (line.empty()) continue;

			if (line == "}") {
				return; // End of block
			}

			lineHandler(line);
		}
		THROW_CONFIG_ERROR(EINVAL, "Unclosed " + blockType + " block.");
	}

	bool isValidPath(const string& rawPath) {
		fs::path path = rawPath;
		// Allow only absolute paths starting with a slash '/'
		if (path.string() == "/") {
			return true;
		}
		// Check if the path is empty or does not start with a slash '/'
		if (path.string().empty() || path.string().front() != '/') {
			return false;
		}
		// Check for multiple consecutive slashes like "//"
		for (size_t i = 1; i < path.string().size(); ++i) {
			if (path.string()[i] == '/' && path.string()[i - 1] == '/') {
				return false;
			}
		}
		// Check for directory traversal sequences like "/../" or "/.."
		if (path == "/.." || path.string().find("/../") != string::npos || path.string().ends_with("/..")) {
			return false;
		}
		return true;
	}

	fs::path computeFilePath(const Location& loc, const string& requestPath) {
		return loc.root / requestPath.substr(loc.path.size());
	}

	string getFileExtension(const string& filePath) {
		size_t dotPos = filePath.find_last_of('.');
		if (dotPos == string::npos) {
			return "";
		}
		return filePath.substr(dotPos + 1);
	}

	bool hasRequiredMethods(const std::vector<std::string>& methods) {
		std::vector<std::string> requiredMethods = {"GET", "POST"};
		return std::all_of(requiredMethods.begin(), requiredMethods.end(), [&methods](const std::string& method) {
			return std::find(methods.begin(), methods.end(), method) != methods.end();
		});
	}

	void parseHeaderFields(
		std::unordered_map<std::string, std::string>& headerFields,
		std::vector<std::uint8_t>& buffer
	) {
		std::string_view emptyLine("\r\n\r\n");
		auto it = std::search(buffer.begin(), buffer.end(), emptyLine.begin(), emptyLine.end());

		if (it == buffer.end()) {
			return;
		}

		std::string rawHeader(buffer.begin(), it + 4);
		std::istringstream istream(rawHeader);
		std::string line;

		while (std::getline(istream, line) && line != "\r") {
			std::size_t colonPos = line.find(":");
			std::string name = line.substr(0, colonPos);
			std::string value = trimSpace(line.substr(colonPos + 1));
			headerFields[name] = value;
		}

		buffer.erase(buffer.begin(), it + 4);
	}

	// FOR TESTING

	// void printRequest(const http::Request& request) {
	// 	cout
	// 		<< "Request method: " << request.getMethod() << endl
	// 		<< "Request URL_scheme: " << request.getUrl().scheme << endl
	// 		<< "Request URL_user: " << request.getUrl().user << endl
	// 		<< "Request URL_password: " << request.getUrl().password << endl
	// 		<< "Request URL_host: " << request.getUrl().host << endl
	// 		<< "Request URL_port: " << request.getUrl().port << endl
	// 		<< "Request URL_path: " << request.getUrl().path << endl
	// 		<< "Request URL_query: " << request.getUrl().query << endl
	// 		<< "Request URL_fragment: " << request.getUrl().fragment << endl
	// 		<< "Request version: " << request.getVersion() << endl
	// 		<< "Request body size: " << request.getRawBody().size() << endl;
	// }

	// void printServerConfig(const ServerConfig& server) {
	// 	cout
	// 		<< YELLOW "Server Config: " RESET << server.serverName << endl
	// 		<< "Host: " << server.host << endl;
	// 	for (const auto& port : server.ports) {
	// 		cout << "Port: " << port << endl;
	// 	}
	// 	cout << "Server Name: " << server.serverName << endl
	// 		<< "Error Pages: " << endl;
	// 	for (const auto& [code, path] : server.errorPages) {
	// 		cout << code << ": " << path << " " << endl;
	// 	}
	// 	cout << endl;
	// 	cout << "Client Max Body Size: " << server.clientMaxBodySize << endl;

	// 	for (const auto& location : server.locations) {
	// 		cout
	// 			<< "Location Path: " << location.path << endl
	// 			<< "Root: " << location.root << endl
	// 			<< "Index: " << location.index << endl
	// 			<< "Autoindex: " << (location.isAutoIndex ? "on" : "off") << endl
	// 			<< "Methods: ";
	// 		for (const auto& method : location.methods) {
	// 			cout << method << " ";
	// 		}
	// 		cout << endl;
	// 		if (!location.cgiExtension.empty()) {
	// 			cout << "CGI Extension: ";
	// 			for (const auto& ext : location.cgiExtension) {
	// 				cout << ext << " ";
	// 			}
	// 			cout << endl;
	// 		}
	// 		if (!location.returnUrl.empty()) {
	// 			cout << "Return URL: ";
	// 			for (const auto& part : location.returnUrl) {
	// 				cout << part << " ";
	// 			}
	// 			cout << endl;
	// 		}
	// 		cout << endl;
	// 	}
	// 	cout << "----------------------------------------" << endl;
	// }

	// void printConfig(const Config& config) {
	// 	for (const auto& server : config.servers) {
	// 		printServerConfig(server);
	// 	}
	// }
} // namespace utils
