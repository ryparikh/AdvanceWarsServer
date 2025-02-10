#include "Platform.h"
#include <iostream>
#include <objbase.h>

namespace Platform {

std::string createUuid() {
	GUID guid;
	HRESULT hCreateGuid = CoCreateGuid(&guid);
	if (hCreateGuid != S_OK) {
		std::cerr << "Failed to create GUID" << std::endl;
		throw;
	}

	// Convert GUID to a string
	std::string gameId;
	RPC_CSTR szUuid = NULL;
	if (::UuidToStringA(&guid, &szUuid) == RPC_S_OK) {
		gameId = (char*)szUuid;
		::RpcStringFreeA(&szUuid);
	}

	return gameId;
}

}