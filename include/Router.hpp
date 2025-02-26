#pragma once

#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

#include "http/index.hpp"
#include "Config.hpp"

// Forward declaration
void handleGetRequest(const Location& loc, const std::string& requestPath, http::Request& request, http::Response& response);
void handlePostRequest(const Location& loc, const std::string& requestPath, http::Request& request, http::Response& response);
void handleDeleteRequest(const Location& loc, const std::string& requestPath, http::Request& request, http::Response& response);
//void handleCgiRequest(const Location& loc, const std::string& requestPath, http::Request& request, http::Response& response);

class Router {
	public:
		using Handler = std::function<void(const Location&, const std::string&, http::Request&, http::Response&)>;

		Router(const ServerConfig& serverConfig) : _serverConfig(serverConfig) {
			addLocations(serverConfig);
		}

		void get(Handler handler);
		void post(Handler handler);
		void del(Handler handler);
		void setCgiHandler(Handler handler) {
			_cgiHandler = handler;
		}
		void handle(http::Request& req, http::Response& res);

		void addLocations(const ServerConfig& serverConfig);
		bool isCGI(const Location& loc, const std::string& requestPath) const;

	private:
		ServerConfig _serverConfig;
		std::string requestPath;
		Handler _cgiHandler;
		std::unordered_map<std::string, Handler> _routes; // method -> handler
		std::unordered_map<std::string, Location> _locationConfigs; // route -> location config

		const Location* findBestMatchingLocation(const std::string& url) const;
};
