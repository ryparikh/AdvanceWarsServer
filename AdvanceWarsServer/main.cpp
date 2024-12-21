#include <iostream>

#include "AdvanceWarsServer.h"

int main() noexcept {
	try {
		AdvanceWarsServer& awaiServer = AdvanceWarsServer::getInstance();
		awaiServer.run();
	}
	catch (...) {
		std::cout << "exception was thrown" << std::endl;
	}
	return 0;
}
