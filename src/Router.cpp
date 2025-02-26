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

namespace {
	void handleMultipartPostRequest(fs::path uploadPath, fs::path rootPath, Request& req, Response& res) {
		auto elements = http::parseMultipart(req.getRawBody(), req.getBoundary());
		std::string responseMessage;

		for (auto& element : elements) {
			try {
				std::ofstream file(uploadPath.string() + utils::generate_random_string() + "_" + element.fileName, std::ios::binary);

				if (!file) {
					std::cerr<< YELLOW "Failed to open file" RESET << std::endl;
					res.setFile(StatusCode::INTERNAL_SERVER_ERROR_500, rootPath / "500.html");
					return;
				}

				file.write(reinterpret_cast<const char*>(element.rawData.data()), element.rawData.size());
				file.close();
				responseMessage += "File '" + element.fileName + "' uploaded successfully\r\n";
			} catch (const std::exception& e) {
				res.setFile(http::StatusCode::INTERNAL_SERVER_ERROR_500, rootPath / "500.html");
				return;
			}
		}

		res.setText(http::StatusCode::OK_200, responseMessage);
	}
}

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


const Location* Router::findBestMatchingLocation(const string& url) const {
	const Location* bestMatch = nullptr;
	size_t longestMatch = 0;
	for (const auto& [path, config] : _locationConfigs) {
		if (url.substr(0, path.size()) == path && path.length() > longestMatch) {
			bestMatch = &config;
			longestMatch = path.length();
		}
	}
	std::cout << "Best matching location: " << bestMatch->path << std::endl;
	return bestMatch;
}

std::string getFileExtension(const std::string& filePath) {
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos == std::string::npos) {
        return "";
    }
    return filePath.substr(dotPos + 1);
}

std::string generateDirectoryListing(const std::string& path) {
	string listing = "<html><head><title>Directory Listing</title></head><body><h1>Directory Listing</h1><ul>";

	for (const auto& entry : fs::directory_iterator(path)) {
		const string& name = entry.path().filename().string();
		listing += "<li><a href=\"" + name + "\">" + name + "</a></li>";
	}

	listing += "</ul></body></html>";
	return listing;
}

fs::path computeFilePath(const Location& loc, const string& requestPath) {
	return loc.root / requestPath.substr(loc.path.size());
}

void handleDirectoryRequest(const Location& loc, const fs::path& filePath, Response& res) {
	fs::path indexPath = filePath / loc.index;
	if (!fs::exists(indexPath)) {
		indexPath = loc.root / loc.index;
	}

	if (fs::exists(indexPath)) {
		res.setFile(StatusCode::OK_200, indexPath);
	} else if (loc.isAutoIndex) {
		res.setFile(StatusCode::OK_200, generateDirectoryListing(filePath));
	} else {
		res.setFile(StatusCode::FORBIDDEN_403, loc.root / "403.html");
	}
}

void handleRedirectRequest(const Location& loc, Response& res, Request& req) {
	std::string redirectUrl = loc.returnUrl[1]; // Extract redirect URL
	res.setHeader(http::Header::LOCATION, redirectUrl);
	res.setStatusCode(http::StatusCode::MOVED_PERMANENTLY_301);
	res.build();
	req.setStatus(Request::Status::COMPLETE);
}

// Function to handle GET requests
void handleGetRequest(const Location& loc, const string& requestPath, Request& req, Response& res) {
	(void) req;
	try {
		// Compute the full file path by appending the request subpath
		fs::path filePath = computeFilePath(loc, requestPath);
		if (fs::is_directory(filePath)) {
			std::cout << YELLOW "Directory request detected" RESET << std::endl;
			handleDirectoryRequest(loc, filePath, res);
		} else if (fs::is_regular_file(filePath)) {
			std::cout << YELLOW "File request detected" RESET << std::endl;
			res.setFile(StatusCode::OK_200, filePath);
		} else {
			std::cout << YELLOW "File not found" RESET << std::endl;
			res.setFile(StatusCode::NOT_FOUND_404, loc.root / "404.html");
		}
	} catch (const std::exception& e) {
		res.setFile(StatusCode::INTERNAL_SERVER_ERROR_500, loc.root / "500.html");
	}
}

// Function to handle POST requests
void handlePostRequest(const Location& loc, const string& requestPath, Request& req, Response& res) {
	fs::path uploadPath = computeFilePath(loc, requestPath);

	if (req.isMultipart()) {
		return handleMultipartPostRequest(uploadPath, loc.root, req, res);
	}

	try {
		const std::string& contentType = req.getHeader(http::Header::CONTENT_TYPE).value_or("");
		const std::string& ext = http::getExtensionFromMimeType(contentType);

		std::ofstream file(uploadPath.string() + utils::generate_random_string() + ext, std::ios::binary);

		if (!file) {
			std::cerr << YELLOW "Failed to open file" RESET << std::endl;
			res.setFile(StatusCode::INTERNAL_SERVER_ERROR_500, loc.root / "500.html");
			return;
		}

		file.write(reinterpret_cast<const char*>(req.getRawBody().data()), req.getRawBody().size());
		res.setText(http::StatusCode::OK_200, "File uploaded successfully\n");
	} catch (const std::exception& e) {
		res.setFile(http::StatusCode::INTERNAL_SERVER_ERROR_500, loc.root / "500.html");
	}
}

// Function to handle DELETE requests
void handleDeleteRequest(const Location& loc, const string& requestPath, Request& req, Response& res) {
	(void) req;
	try {
		fs::path filePath = computeFilePath(loc, requestPath);
		if (!fs::exists(filePath)) {
			res.setFile(http::StatusCode::NOT_FOUND_404, loc.root / "404.html");
			return;
		}
		if (fs::remove(filePath)) {
			res.setText(http::StatusCode::OK_200, "File deleted successfully");
			return;
		} else {
			res.setFile(http::StatusCode::INTERNAL_SERVER_ERROR_500, loc.root / "500.html");
			return;
		}
	} catch (const std::exception& e) {
		res.setFile(http::StatusCode::INTERNAL_SERVER_ERROR_500, loc.root / "500.html");
	}
}

bool hasRequiredMethods(const std::vector<std::string>& methods) {
    std::vector<std::string> requiredMethods = {"GET", "POST"};
    return std::all_of(requiredMethods.begin(), requiredMethods.end(), [&methods](const std::string& method) {
        return std::find(methods.begin(), methods.end(), method) != methods.end();
    });
}

bool Router::isCGI(const Location& location, const std::string& requestPath) const {
	std::cout << "Checking for CGI" << std::endl;
    // 1. Check if CGI is enabled in this location
    if (location.cgiExtension.empty()) {
		std::cout << "CGI extension is empty" << std::endl;
        return false;
    }

    // 2. Extract file extension from request path
    size_t dotPos = requestPath.find_last_of('.');
    if (dotPos == std::string::npos) {
		std::cout << "No file extension found" << std::endl;
        return false;  // No file extension found
    }

    std::string ext = requestPath.substr(dotPos + 1);
	std::cout << "File extension: " << ext << std::endl;

    // 3. Check if the file extension is in the cgiExtension vector
    bool isCgiExtension = false;
    for (const auto& cgiExt : location.cgiExtension) {
        if (ext == cgiExt) {
            isCgiExtension = true;
            break;
        }
    }

    if (!isCgiExtension) {
        std::cout << "File extension is not a valid CGI script type" << std::endl;
        return false;  // File extension is not a valid CGI script type
    }

    // 4. Compute the full path of the script
    fs::path scriptPath = location.root / requestPath.substr(location.path.size());
	std::cout << "Script path: " << scriptPath << std::endl;
    // 5. Check if the requested file exists and is executable
    if (!fs::exists(scriptPath) || !fs::is_regular_file(scriptPath)) {
		std::cout << "Script does not exist or is not a regular file" << std::endl;
        return false;
    }

    // 6. Check if the file has execute permissions for the owner
    fs::perms permissions = fs::status(scriptPath).permissions();
    if ((permissions & fs::perms::owner_exec) == fs::perms::none) {
		std::cout << "Script does not have execute permissions" << std::endl;
        return false;
    }
	std::cout << "CGI detected and set to true" << std::endl;
    return true;
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
