#pragma once

#define BACKLOG 128

#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <poll.h>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>

#include "Config.hpp"
#include "http/index.hpp"
#include "Router.hpp"

struct WorkerProcess {
	int pipeFds[2];
	int clientFd;
	pid_t pid;
	std::filesystem::path rootPath;
};

class Server {
	public:
		Server() = delete;
		Server(const ServerConfig& serverConfig);
		~Server();

		Server(Server&&) noexcept = default;
		Server& operator=(Server&&) noexcept = default;

		void addRouterHandlers();
		void closeConnection(http::Connection& con);
		void process(const int fd, short& events, const short revents);

		const std::unordered_set<int>& getServerFds() const;
		std::unordered_map<int, http::Connection> connections;
		std::unordered_map<int, WorkerProcess> workerProcesses;
		std::vector<pid_t> unreapedProcesses;
		void shutdown();

	private:
		const ServerConfig& _serverConfig;
		Router _router;
		std::unordered_set<int> _serverFds;

		void _handleCGI(
			const Location& loc,
			const std::string& requestPath,
			const http::Request& request,
			http::Response& response
		);

		void _processConnection(http::Connection& con, short& events, const short revents);
		void _processWorkerProcess(WorkerProcess& process, const short revents);
		void _cleanup();
};
