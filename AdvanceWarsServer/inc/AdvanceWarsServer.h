#pragma once

#include <array>
#include <iostream>
#include <thread>
#include <chrono>

#include "CommandingOfficier.h"
#include "GameState.h"
#include "Map.h"
#include "MapParser.h"
#include "MovementTypes.h"
#include "Result.h"
#include "Terrain.h"

#include "via/http/request_router.hpp"
#include "via/comms/tcp_adaptor.hpp"
#include "via/http_server.hpp"
#include "nlohmann/json.hpp"

using namespace via::http;
using json = nlohmann::json;

using time_point = std::chrono::time_point<std::chrono::steady_clock>;

constexpr std::chrono::duration<double, std::milli> framerate = std::chrono::milliseconds(1000 / 60);

class AdvanceWarsServer {
public:
	/// Define an HTTP server using std::string to store message bodies
	typedef via::http_server<via::comms::tcp_adaptor, std::string> http_server_type;
	typedef http_server_type::http_connection_type http_connection;
	typedef http_server_type::http_request http_request;

	AdvanceWarsServer();
	int run();

	static AdvanceWarsServer& getInstance();
	static tx_response post_games_handler(const http_request& request, const Parameters& parameters, const std::string& data, std::string& response_body);
	static tx_response post_game_actions(const http_request& request, const Parameters& parameters, const std::string&data, std::string&response_body);
	static tx_response get_game_actions(const http_request& request, const Parameters& parameters, const std::string&data, std::string&response_body);
//	static tx_response get_valid_game_actions(const http_request& request, const Parameters& parameters, const std::string&data, std::string&response_body);

	json create_new_game(std::string guid);
	json get_actions(const std::string& gameId, int x, int y) const;
	json do_action(const std::string& gameId, int x, int y, const Action& action);
private:
	static std::unique_ptr<AdvanceWarsServer> s_spServer;
	asio::io_context m_io_context;
	http_server_type m_http_server;
	std::map<std::string, std::unique_ptr<GameState>> m_gameCache;
};
