#include "Router.hpp"

using std::string;
namespace fs = std::filesystem;

bool Router::isCGI(const Location& location, const string& requestPath) const {
	std::cout << "Checking for CGI" << std::endl;
	// 1. Check if CGI is enabled in this location
	if (location.cgiExtension.empty()) {
		std::cout << "CGI extension is empty" << std::endl;
		return false;
	}

	// 2. Extract file extension from request path
	size_t dotPos = requestPath.find_last_of('.');
	if (dotPos == string::npos) {
		std::cout << "No file extension found" << std::endl;
		return false;  // No file extension found
	}

	string ext = requestPath.substr(dotPos + 1);
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