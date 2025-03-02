#pragma once

#include <functional>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <unordered_map>

# define YELLOW "\033[0;33m"
# define GREEN "\033[0;32m"
# define BLUE "\033[0;34m"
# define RESET "\033[0m"

struct Location {
	std::string path;						// Location path (/, /static/, /static/index.html, /cgi-bin)
	std::filesystem::path root;				// Full path resolved during parsing
	std::string index;						// Default index.html file
	std::string autoIndex;
	bool isAutoIndex = false;				// Enbale or disable directory listing
	std::vector<std::string> methods; 		// Allowed methods
	std::vector<std::string> cgiExtension; 	// CGI extensions
	std::vector<std::string> returnUrl; 	// Redirect URLs (if any)
};

struct ServerConfig {
	std::string host;
	std::vector<int> ports;
	std::string serverName;
	std::map<int, std::string> errorPages;
	std::string clientMaxBodySizeStr;
	size_t clientMaxBodySize = 10 * 1024 * 1024;	// 10MB
	std::vector<Location> locations;

	std::size_t msRequestTimeout = 10000;			// Default: 10 seconds
	std::size_t msResponseHandlingTimeout = 5000;	// Default: 5 seconds
	std::size_t msResponseDeliveryTimeout = 10000;	// Default: 10 seconds
	std::size_t msIdleTimeout = 5000;				// Default: 5 seconds
};

// Multi servers struct
struct Config {
	std::vector<int> ports;
	std::vector<ServerConfig> servers;
};

// Define types for parsers
using ParserFunction = std::function<void(const std::string&)>;
using ParserMap = std::unordered_map<std::string, ParserFunction>;

class ConfigParser {
	public:
		using LineHandler = std::function<void(const std::string&)>;
		ConfigParser() = default;
		ConfigParser(const std::string &path): filePath(path) {};

		void parseHttpBlock(std::ifstream &file, Config &config);
		void parseServerBlock(std::ifstream &file, ServerConfig &server);
		void parseLocationBlock(std::ifstream &file, Location &location);
		void parseConfig(const std::string &filename, Config &config);
		void parseGlobal(const std::string &line, ServerConfig &server);
		void parseLocation(const std::string &line, Location &currentLocation);
		Config load();
		std::filesystem::path getConfigPath(const std::string &value) const;

	private:
		std::filesystem::path filePath;
		std::filesystem::path fullPath;
};
