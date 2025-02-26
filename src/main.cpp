#include <iostream>
#include "Config.hpp"
#include "Error.hpp"
#include "Server.hpp"
#include <exception>
#include <thread>
#include "SignalHandle.hpp"
#include "ServerManager.hpp"
#include "http/index.hpp"

int main(int argc, char **argv) {
	if (argc != 2) {
		std::cerr << "Error: Invalid number of arguments!" << std::endl;
		return(1);
	}

	try {
		handleSignals();
		ConfigParser parser(argv[1]);
		Config config = parser.load();
		ServerManager serverManager(config);
		serverManager.listen();
		serverManager.shutdown();
	} catch (const WSException& e) {
		std::cerr << "Error: " << e.code() << " " << e.code().message() << std::endl;
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}
