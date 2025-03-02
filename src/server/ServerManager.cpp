#include <sys/socket.h>
#include <sys/wait.h>
#include "ServerManager.hpp"
#include "utils/index.hpp"
#include "SignalHandle.hpp"

ServerManager::ServerManager(const Config& config) : _config(config) {
	_servers.reserve(_config.servers.size());

	for (std::size_t i = 0; i < _config.servers.size(); i++) {
		const ServerConfig& serverConfig = _config.servers[i];
		_servers.push_back(Server(serverConfig));
		Server& server = _servers.back();
		server.addRouterHandlers();
		server.onShutdown([this]() {
			this->shutdown();
		});
    }

	for (auto& server : _servers) {
		for (const int serverFd : server.getServerFds()) {
			_track(serverFd, server);
		}
	}
}

void ServerManager::listen() {
	while (_pollFds.size() > 0 && !isInterrupted) {
        int ret = ::poll(_pollFds.data(), _pollFds.size(), 100);

		if (ret == -1 && errno == EINTR) {
			break;
		}

		if (ret > 0) {
			for (auto& [fd, events, revents] : _pollFds) {
				auto& server = _serverMap.at(fd).get();
				server.process(fd, events, revents);
			}
		}

		_updatePollFds();
    }
}

void ServerManager::_updatePollFds() {
	for (auto& server: _servers) {
		_updateClientConnections(server);
		_updatePipeConnections(server);

		for (auto it = server.unreapedProcesses.begin(); it != server.unreapedProcesses.end();) {
			pid_t pid = ::waitpid(*it, NULL, WNOHANG);

			if (pid > 0 || (pid == -1 && errno == ECHILD)) {
				it = server.unreapedProcesses.erase(it);
				continue;
			}

			it++;
		}
	}
}

void ServerManager::_updateClientConnections(Server& server) {
	for (auto it = server.connections.begin(); it != server.connections.end();) {
		auto& [fd, connection] = *it;

		if (!connection.isClosed()) {
			if (!_pollfdIndexMap.contains(fd)) {
				_track(fd, server);
				it++;
				continue;
			}

			if (connection.isTimedOut()) {
				std::cout << "clientFd " << connection.getClientFd() << " has timedout" << std::endl;
				server.closeConnection(connection);
			}
		}

		if (connection.isClosed()) {
			std::cout << "clientFd " << fd << " has closed" << std::endl;
			_untrack(fd);
			it = server.connections.erase(it);
			continue;
		}

		it++;
	}
}

void ServerManager::_updatePipeConnections(Server& server) {
	for (auto it = server.workerProcesses.begin(); it != server.workerProcesses.end();) {
		auto& [fd, process] = *it;
		const bool isBeingTracked = _pollfdIndexMap.contains(fd);

		if (process.pipeFds[0] != -1 && !isBeingTracked) {
			_track(fd, server);
			it++;
			continue;
		}

		if (process.pipeFds[0] == -1) {
			auto conIt = server.connections.find(process.clientFd);

			if (
				conIt != server.connections.end()
				&& (conIt->second.getResponse() != nullptr)
				&& conIt->second.getResponse()->getStatus() == http::Response::Status::READY
			) {
				const std::size_t index = _pollfdIndexMap[process.clientFd];
				_pollFds[index].events |= POLLOUT;
			}

			if (isBeingTracked) {
				_untrack(fd);
			}

			it = server.workerProcesses.erase(it);
			continue;
		}

		it++;
	}
}

void ServerManager::shutdown() {
	for (auto& server : _servers) {
		server.shutdown();
	}

	while (1) {
		if (::wait(NULL) == -1 && errno == ECHILD) {
			break;
		}
	}
}

void ServerManager::_track(int fd, Server& server) {
	auto it = _pollfdIndexMap.find(fd);

	if (it != _pollfdIndexMap.end()) {
		return;
	}

	_pollFds.push_back({ fd, POLLIN, 0 });
	_pollfdIndexMap[fd] = _pollFds.size() - 1;
	_serverMap.emplace(fd, server);
}

void ServerManager::_untrack(int fd)  {
	auto it = _pollfdIndexMap.find(fd);

	if (it == _pollfdIndexMap.end()) {
		return;
	}

	const std::size_t index = it->second;

	_pollfdIndexMap.erase(it);
	_serverMap.erase(fd);

	if (index != _pollFds.size() - 1) {
		std::swap(_pollFds[index], _pollFds.back());
		_pollfdIndexMap[_pollFds[index].fd] = index;
	}

	_pollFds.pop_back();
}
