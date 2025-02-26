#include "utils/common.hpp"
#include "Config.hpp"
#include "Error.hpp"
//#include "Server.hpp"
#include <sstream> // std::istringstream

// Define namespaces
using std::string;
using std::vector;
using std::ifstream;
using std::istringstream;
using std::cout;
using std::endl;
namespace fs = std::filesystem;

void ConfigParser::parseHttpBlock(ifstream &file, Config &config) {
	utils::parseBlock(file, "http", [&](const string &line) {
		if (line == "server {") {
			ServerConfig server;
			parseServerBlock(file, server);
			config.servers.push_back(server);
		} else {
			THROW_CONFIG_ERROR(EINVAL, "Invalid directive in http block: " + line);
		}
	});
}

void ConfigParser::parseServerBlock(ifstream &file, ServerConfig &server) {
	utils::parseBlock(file, "server", [&](const string &line) {
		if (line.find("location ") == 0) {
		Location location;
		auto [key, path] = utils::splitKeyValue(line);
		location.path = path;
		parseLocationBlock(file, location);
		server.locations.push_back(location);
		} else {
			parseGlobal(line, server);
		}
	});
	}

void ConfigParser::parseLocationBlock(ifstream &file, Location &location) {
	utils::parseBlock(file, "location", [&](const string &line) {
		parseLocation(line, location);
	});
	}

	// Function to parse global configuration lines
void ConfigParser::parseGlobal(const string &line, ServerConfig &server) {
	const ParserMap globalParsers = {
		{"host", [&](const string &value) {
			if (!server.host.empty() || !utils::isValidIP(value)) {
				THROW_CONFIG_ERROR(EINVAL, "Invalid host");
			}
			server.host = value;
		}},
		{"port", [&](const string &value) {
			server.ports.push_back(utils::parsePort(value));
		}},
		{"server_name", [&](const string &value) {
			if (!server.serverName.empty()) {
				THROW_CONFIG_ERROR(EINVAL, "Invalid server_name");
			}
			server.serverName = value;
		}},
		{"error_page", [&](const string &value) {
			istringstream iss(value);
			std::string code, path;
			iss >> code >> path;
			fullPath = getConfigPath(path);
			utils::validateErrorPage(code, fullPath);
			server.errorPages[std::stoi(code)] = fullPath;
		}},
		{"client_max_body_size", [&](const string &value) {
			if (!server.clientMaxBodySizeStr.empty() || !utils::isValidSize(value)) {
				THROW_CONFIG_ERROR(EINVAL, "Invalid client_max_body_size");
			}
			server.clientMaxBodySize = utils::convertSizeToBytes(value);
		}}
	};

	utils::parseKeyValue(line, globalParsers);
}

// filePath as argv[1] = config/webserv.conf
// value: /static/
// parent_path: /Users/username/Webserv/config/
// return: /Users/username/Webserv/config/static/
fs::path ConfigParser::getConfigPath(const string &value) const {
	if (filePath.empty() || value.empty()) {
		THROW_CONFIG_ERROR(EINVAL, "Invalid path");
	}
	fs::path fullPath = utils::sanitizePath(filePath.parent_path(), value);
	return fullPath;

}

void ConfigParser::parseLocation(const string &line, Location &currentLocation) {
	const ParserMap locationParsers = {
		{"root", [&](const string &value) {
			fullPath = getConfigPath(value);
			if (!currentLocation.root.empty() || !utils::isValidFilePath(fullPath)) {
				THROW_CONFIG_ERROR(EINVAL, "Invalid root");
			}
			currentLocation.root = fullPath;
		}},
		{"index", [&](const string &value) {
			fullPath = currentLocation.root/value;
			if (!currentLocation.index.empty() || !utils::isValidFilePath(fullPath)) {
				THROW_CONFIG_ERROR(EINVAL, "Invalid index");
			}
			currentLocation.index = value;
		}},
		{"autoindex", [&](const string &value) {
			if (!currentLocation.autoIndex.empty()) {
				THROW_CONFIG_ERROR(EINVAL, "Invalid autoindex");
			}
			currentLocation.autoIndex = value;
			currentLocation.isAutoIndex = utils::parseBool(value);
		}},
		{"methods", [&](const string &value) {
			if (!currentLocation.methods.empty()) {
				THROW_CONFIG_ERROR(EINVAL, "Invalid methods");
			}
			istringstream iss(value);
			vector<string> methods;
			string method;
			while (iss >> method) {
				methods.push_back(method);
			}
			utils::validateMethods(methods);
			currentLocation.methods = methods;
		}},
		{"cgi_extension", [&](const string &value) {
			std::string extension = value;
			if (extension[0] == '.') {
				extension = extension.substr(1);  // Remove leading dot
			}
			currentLocation.cgiExtension.push_back(extension);
			std::cout << "CGI extension: " << extension << std::endl;
		}},
		{"return", [&](const string &value) {
			if (!currentLocation.returnUrl.empty()) {
				THROW_CONFIG_ERROR(EINVAL, "Invalid return");
			}
			istringstream iss(value);
			vector<string> returnParts;
			string part;
			while (iss >> part) {
				returnParts.push_back(part);
			}
			if (returnParts.size() != 2 || !utils::isValidURL(returnParts[1])) {
				THROW_CONFIG_ERROR(EINVAL, "Invalid return");
			}
			currentLocation.returnUrl = returnParts;
		}}
	};

	utils::parseKeyValue(line, locationParsers);
}

void ConfigParser::parseConfig(const string &filename, Config& config) {
	ifstream file(filename);
	if (!file.is_open()) {
		THROW_CONFIG_ERROR(ENOENT, "Unable to open configuration file.");
	}

	string line;
	while (std::getline(file, line)) {
		line = utils::trim(utils::removeComments(line));
		if (line.empty()) continue;

		if (line == "http {") {
			parseHttpBlock(file, config);
		} else {
			THROW_CONFIG_ERROR(EINVAL, "Invalid directive outside of blocks: " + line);
		}
	}
}


// Function to load the configuration
Config ConfigParser::load() {
	Config config;
	parseConfig(filePath, config);
	//utils::printConfig(config); // For testing
	return config;
}
