#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <unistd.h>

#include "Server.hpp"
#include "utils/index.hpp"
#include "SignalHandle.hpp"

Server::Server(const ServerConfig& serverConfig) : _serverConfig(serverConfig) , _router(serverConfig) {
	_serverFds.reserve(serverConfig.ports.size());

	for (const int port : serverConfig.ports) {
		int serverFd = utils::createPassiveSocket(serverConfig.host.data(), port, 128, true);
		std::cout << "listening on " << serverConfig.host << ":" << port << std::endl;
		_serverFds.emplace(serverFd);
	}
}

void Server::addRouterHandlers() {
	_router.get(handleGetRequest);
	_router.post(handlePostRequest);
	_router.del(handleDeleteRequest);
	_router.setCgiHandler([this](const Location& loc, const std::string& requestPath, http::Request& req, http::Response& res) {
		this->_handleCGI(loc, requestPath, req, res);
	});
}

void Server::onShutdown(std::function<void()> shutdownHandler) {
	_shutdownHandler = shutdownHandler;
}

void Server::closeConnection(http::Connection& con) {
	int clientFd = con.getClientFd();
	con.close();

	for (auto& [_, process] : workerProcesses) {
		if (process.clientFd == clientFd) {
			if (::kill(process.pid, 0) == 0) {
				::kill(process.pid, SIGTERM);
			}

			if (process.pipeFds[0] != -1) {
				::close(process.pipeFds[0]);
				process.pipeFds[0] = -1;
			}

			pid_t pid = ::waitpid(process.pid, NULL, WNOHANG);

			if (pid == 0) {
				unreapedProcesses.push_back(pid);
			}
		}
	}
}

void Server::process(const int fd, short& events, const short revents) {
	if (_serverFds.contains(fd) && revents == POLLIN) {
		sockaddr_in clientAddr {};
		socklen_t addrLen = sizeof(clientAddr);
		int clientFd = ::accept(fd, (struct sockaddr*)&clientAddr, &addrLen);

		if (clientFd >= 0) {
			std::cout << "clientFd " << fd << " has connected" << std::endl;
			connections.emplace(clientFd, http::Connection(clientFd, _serverConfig));
		}

		return;
	}

	if (auto it = connections.find(fd); it != connections.end()) {
		return _processConnection(it->second, events, revents);
	}

	if (auto it = workerProcesses.find(fd); it != workerProcesses.end()) {
		return _processWorkerProcess(it->second, revents);
	}
}

const std::unordered_set<int>& Server::getServerFds() const {
	return _serverFds;
}

void Server::_processConnection(http::Connection& con, short& events, const short revents) {
	if (con.isClosed()) {
		return;
	}

	using enum http::Response::Status;

	if (revents & POLLHUP) {
		closeConnection(con);
		return;
	}

	if (revents & POLLIN) {
		con.read();
		auto* req = con.getRequest();
		auto* res = con.getResponse();

		if (req != nullptr && res != nullptr && res->getStatus() == PENDING) {
			res->setStatus(IN_PROGRESS);
			_router.handle(*req, *res);

			if (res->getStatus() == READY) {
				events |= POLLOUT;
			}
		}
	}

	if ((revents & POLLOUT) && con.sendResponse()) {
		events &= ~POLLOUT;
	}
}

void Server::_processWorkerProcess(WorkerProcess& process, const short revents) {
	if (process.pipeFds[0] == -1) {
		return;
	}

	http::Response* res = connections.at(process.clientFd).getResponse();

	if (res == nullptr || res->getStatus() == http::Response::Status::READY) {
		return;
	}

	bool hasError = false;

	if (revents & POLLIN) {
		unsigned char buffer[4096];
		ssize_t bytesRead = ::read(process.pipeFds[0], buffer, sizeof(buffer));

		if (bytesRead > 0) {
			std::string_view errorMarker("EXECVE_ERROR_MARKER");
			auto it = std::search(buffer, buffer + bytesRead, errorMarker.begin(), errorMarker.end());

			if (it != (buffer + bytesRead)) {
				hasError = true;
				std::cerr << "CGI failed" << std::endl;
			} else {
				res->appendBody(buffer, bytesRead);
			}
		}
	}

	if (revents & POLLHUP) {
		if (hasError) {
			res->clear().setFile(http::StatusCode::INTERNAL_SERVER_ERROR_500, process.rootPath / "500.html");
		} else {
			auto* cgiPayload = dynamic_cast<utils::CgiPayload*>(res->getBody().get());

			for (auto& [name, value] : cgiPayload->headerFields()) {
				res->setHeader(name, value);
			}

			res->setStatusCode(http::StatusCode::OK_200)
				.setHeader(http::Header::CONTENT_LENGTH, std::to_string(cgiPayload->size()))
				.build();
		}

		::close(process.pipeFds[0]);
		process.pipeFds[0] = -1;

		if (::kill(process.pid, 0) == 0) {
			::kill(process.pid, SIGTERM);
		}

		pid_t pid = ::waitpid(process.pid, NULL, WNOHANG);

		if (pid == 0) {
			unreapedProcesses.push_back(pid);
		}
	}
}

void Server::_handleCGI(
	const Location& loc,
	const std::string& requestPath,
	const http::Request& request,
	http::Response& response
) {
	(void)request;
	std::string scriptPath = loc.root / requestPath.substr(loc.path.size());

	WorkerProcess process;

	process.clientFd = response.getClientSocket();
	process.rootPath = loc.root;

	if (::pipe(process.pipeFds) == -1 || !utils::setNonBlocking(process.pipeFds[0])) {
		response.setFile(http::StatusCode::INTERNAL_SERVER_ERROR_500, loc.root / "500.html");
		return;
	}

	process.pid = ::fork();

	if (process.pid == -1) {
		::close(process.pipeFds[0]);
		::close(process.pipeFds[1]);
		response.setFile(http::StatusCode::INTERNAL_SERVER_ERROR_500, loc.root / "500.html");
		return;
	}

	if (process.pid == 0) {
		::close(process.pipeFds[0]);
		::dup2(process.pipeFds[1], STDOUT_FILENO);
		::close(process.pipeFds[1]);

		std::string interpreter("/usr/bin/python1");
		char* argv[] = { interpreter.data(), scriptPath.data(), NULL };
		char** envp = request.getCgiEnvp();
		::execve(argv[0], argv, envp);

		for (std::size_t i = 0; envp[i] != nullptr; i++) {
			delete[] envp[i];
		}

		delete[] envp;

		std::cout << "EXECVE_ERROR_MARKER";
		std::flush(std::cout);
		_shutdownHandler();
		exit(1);
	}

	::close(process.pipeFds[1]);
	process.pipeFds[1] = -1;
	response.setBody(std::make_unique<utils::CgiPayload>());
	workerProcesses.emplace(process.pipeFds[0], process);
}

void Server::shutdown() {
	for (auto& [_, con] : connections) {
		con.close();
	}

	for (auto& [_, process] : workerProcesses) {
		if (process.pipeFds[0] == -1) {
			::close(process.pipeFds[0]);
			process.pipeFds[0] = -1;
		}

		::kill(process.pid, SIGTERM);
	}

	for (const int fd : _serverFds) {
		::close(fd);
	}

	connections.clear();
	workerProcesses.clear();
}
