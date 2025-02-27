#include "Config.hpp"
#include "Router.hpp"
#include "http/index.hpp"
#include "utils/index.hpp"

using http::StatusCode;
using http::Request;
using http::Response;
using std::string;
namespace fs = std::filesystem;

/* locations path:
** /
** /static/
** /cgi-bin/
** /uploads/
*/

void Router::addLocations(const ServerConfig& serverConfig) {
	_serverConfig = serverConfig;
	for (const auto& location : serverConfig.locations) {
		_locationConfigs[location.path] = location;
	}
}

void Router::get(Handler handler) {
	_routes["GET"] = handler;
}

void Router::post(Handler handler) {
	_routes["POST"] = handler;
}

void Router::del(Handler handler) {
	_routes["DELETE"] = handler;
}

// Hander function to handle requests based on the method and matching location
void Router::handle(Request& request, Response& response) {
	std::cout << "handle(): " << request.getUri() << response.getClientSocket() << std::endl;
	if (request.getStatus() == Request::Status::BAD) {
		response.setFile(StatusCode::BAD_REQUEST_400, _serverConfig.errorPages[400]);
		return;
	}

	// Get the request path and normalize it
	std::string requestPath = utils::lowerCase(request.getUrl().path);
	std::cout << "Request path: " << requestPath << std::endl;
	// Ensure directory paths have a trailing slash, but files do not
	if (!requestPath.empty() && requestPath.back() != '/' && !fs::path(requestPath).has_extension()) {
		requestPath += "/";
	}

	// Validate the request path
	if (!utils::isValidPath(requestPath)) {
		response.setFile(StatusCode::BAD_REQUEST_400, _serverConfig.errorPages[400]);
		return;
	}

	// Find the best matching location
	const Location* location = findBestMatchingLocation(requestPath);
	if (!location) {
		response.setFile(StatusCode::NOT_FOUND_404, _serverConfig.errorPages[404]);
		return;
	}

	// Check for redirect
	if (!location->returnUrl.empty()) {
		handleRedirectRequest(*location, response, request);
		std::cout << "SHOULD STOP HERE" << std::endl;
		return;
	}

	if (isCGI(*location, requestPath)) {
		std::cout << YELLOW "CGI request detected" RESET << std::endl;
		if (_cgiHandler) {  // Ensure handler is set
			_cgiHandler(*location, requestPath, request, response);
		} else {
			std::cerr << "[ERROR] CGI Handler is not registered!" << std::endl;
			response.setFile(StatusCode::INTERNAL_SERVER_ERROR_500, _serverConfig.errorPages[500]);
		}
		return;
	}

	// Find the handler for the requested http method
	const auto it = _routes.find(request.getMethod());

	// Check if the method is allowed via the location config
	const auto it2 = std::find(location->methods.begin(), location->methods.end(), request.getMethod());
	if (it2 == location->methods.end()) {
		response.setFile(StatusCode::METHOD_NOT_ALLOWED_405, _serverConfig.errorPages[405]);
		return;
	}

	// Matched a route
	if (it != _routes.end()) {
		try {
			const auto handler = it->second;
			handler(*location, requestPath, request, response);
			request.setStatus(Request::Status::COMPLETE);
		} catch(const std::exception& e) {
			response.clear();
			response.setFile(StatusCode::INTERNAL_SERVER_ERROR_500, _serverConfig.errorPages[500]);
		}
		return;
	} else {
		response.setFile(StatusCode::METHOD_NOT_ALLOWED_405, _serverConfig.errorPages[405]);
		return;
	}
}
