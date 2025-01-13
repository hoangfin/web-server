#pragma once

#include <string>
#include <functional>
#include "http/Request.hpp"
#include "http/Response.hpp"
#include "Config.hpp"
#include "http/Url.hpp"
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <fstream>

// Forward declaration
void handleGetRequest(const Location& loc, const std::string& requestPath, http::Request& request, http::Response& response);
void handlePostRequest(const Location& loc, const std::string& requestPath, http::Request& request, http::Response& response);
void handleDeleteRequest(const Location& loc, const std::string& requestPath, http::Request& request, http::Response& response);

class Router {
	public:
		using Handler = std::function<void(const Location&, const std::string&, http::Request&, http::Response&)>;

		Router(const ServerConfig& serverConfig) : _serverConfig(serverConfig) {
			addLocations(serverConfig);
		}

		void get(Handler handler);
		void post(Handler handler);
		void del(Handler handler);
		void handle(http::Request& req, http::Response& res);

		void addLocations(const ServerConfig& serverConfig);

	private:
		ServerConfig _serverConfig;
		std::string requestPath;
	
		std::unordered_map<std::string, Handler> _routes; // method -> handler
		std::unordered_map<std::string, Location> _locationConfigs; // route -> location config

		const Location* findBestMatchingLocation(const std::string& url) const;
};
