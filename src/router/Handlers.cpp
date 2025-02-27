#include "Router.hpp"
#include "http/index.hpp"
#include "utils/common.hpp"

using http::StatusCode;
using http::Request;
using http::Response;
using std::string;
namespace fs = std::filesystem;

// Function to generate directory listing
string generateDirectoryListing(const string& path) {
	string listing = "<html><head><title>Directory Listing</title></head><body><h1>Directory Listing</h1><ul>";

	for (const auto& entry : fs::directory_iterator(path)) {
		const string& name = entry.path().filename().string();
		listing += "<li><a href=\"" + name + "\">" + name + "</a></li>";
	}

	listing += "</ul></body></html>";
	return listing;
}

// Function to handle directory requests
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

// Function to handle multipart POST requests
void handleMultipartPostRequest(fs::path uploadPath, fs::path rootPath, Request& req, Response& res) {
	auto elements = http::parseMultipart(req.getRawBody(), req.getBoundary());
	string responseMessage;

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
			res.setFile(StatusCode::INTERNAL_SERVER_ERROR_500, rootPath / "500.html");
			return;
		}
	}

	res.setText(http::StatusCode::OK_200, responseMessage);
}

// Function to handle GET requests
void handleGetRequest(const Location& loc, const string& requestPath, Request& req, Response& res) {
	(void) req;
	try {
		// Compute the full file path by appending the request subpath
		fs::path filePath = utils::computeFilePath(loc, requestPath);
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
	fs::path uploadPath = utils::computeFilePath(loc, requestPath);

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
		fs::path filePath = utils::computeFilePath(loc, requestPath);
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

// Handler function to handle redirect requests
void Router::handleRedirectRequest(const Location& loc, Response& res, Request& req) {
	std::string redirectUrl = loc.returnUrl[1];  // Extract redirect target
	std::cout << YELLOW "Redirecting to: " RESET << redirectUrl << std::endl;

	// Correctly set HTTP 301 redirect
	res.setStatusCode(http::StatusCode::MOVED_PERMANENTLY_301);
	res.setHeader(http::Header::LOCATION, redirectUrl);
	res.setHeader(http::Header::CONTENT_LENGTH, "0");  // Explicitly indicate no body

	// Ensure response is fully constructed and sent
	res.build();
	req.setStatus(Request::Status::COMPLETE);
}