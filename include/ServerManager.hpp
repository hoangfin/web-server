#pragma once

#include <vector>
#include <functional>
#include <unordered_set>

#include "Config.hpp"
#include "Server.hpp"

class ServerManager {
	public:
		ServerManager() = delete;
		ServerManager(const Config& config);
		~ServerManager() = default;
		void listen();
		void shutdown();

	private:
		const Config& _config;
		std::vector<Server>	_servers;
		std::unordered_map<int, std::reference_wrapper<Server>> _serverMap;
		std::vector<struct ::pollfd> _pollFds;
		std::unordered_map<int, std::size_t> _pollfdIndexMap;

		void _track(int fd, Server& server);
		void _untrack(int fd);
		void _updatePollFds();
		void _processPollFds();
		void _updateClientConnections(Server& server);
		void _updatePipeConnections(Server& server);
};
