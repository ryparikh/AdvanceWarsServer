#include "AdvanceWarsServer.h"
#include <objbase.h>

std::unique_ptr<AdvanceWarsServer> AdvanceWarsServer::s_spServer{ nullptr };
/*static*/ AdvanceWarsServer& AdvanceWarsServer::getInstance() {
	if (s_spServer == nullptr) {
		s_spServer.reset(new AdvanceWarsServer());
	}

	return *s_spServer;
}

/*static*/ tx_response AdvanceWarsServer::post_games_handler(const http_request& request, const Parameters& parameters, const std::string& data, std::string& response_body) {
	json gameRequestJson = json::parse(data);

	AdvanceWarsServer& server = AdvanceWarsServer::getInstance();
	GUID guid;
	HRESULT hCreateGuid = CoCreateGuid(&guid);
	if (hCreateGuid != S_OK) {
		std::cerr << "Failed to create GUID" << std::endl;
		tx_response resp(response_status::code::INTERNAL_SERVER_ERROR);
		return resp;
	}
	// Convert GUID to a string
	std::string guidString;
	RPC_CSTR szUuid = NULL;
	if (::UuidToStringA(&guid, &szUuid) == RPC_S_OK) {
		guidString = (char*)szUuid;
		::RpcStringFreeA(&szUuid);
	}

	std::cout << "GUID: " << guidString << std::endl;

	// Free the allocated string
	const json jsonResponseBody = server.create_new_game(guidString);
	response_body = jsonResponseBody.dump();
	tx_response resp(response_status::code::OK);
	resp.add_header("Access-Control-Allow-Origin", "*");
	return resp;
}

/*static*/ tx_response AdvanceWarsServer::get_game_actions(const http_request& request, const Parameters& parameters, const std::string&data, std::string&response_body) {
	int x = std::stoi(parameters.find("x")->second);
	int y = std::stoi(parameters.find("y")->second);
	std::string gameId = parameters.find("gameid")->second;

	AdvanceWarsServer& server = AdvanceWarsServer::getInstance();
	json j = server.get_actions(gameId, x, y);
	response_body = j.dump();

	tx_response resp(response_status::code::OK);
	resp.add_header("Access-Control-Allow-Origin", "*");
	return resp;
}

/*static*/ tx_response AdvanceWarsServer::put_game_action(const http_request& request, const Parameters& parameters, const std::string&data, std::string&response_body) {
	int x = std::stoi(parameters.find("x")->second);
	int y = std::stoi(parameters.find("y")->second);
	std::string gameId = parameters.find("gameid")->second;
	AdvanceWarsServer& server = AdvanceWarsServer::getInstance();

	Action action;
	std::istringstream stream(data);
	json j;
	stream >> j;
	from_json(j, action);

	server.do_action(gameId, x, y, action);
	tx_response resp(response_status::code::OK);
	resp.add_header("Access-Control-Allow-Origin", "*");
	return resp;
}

AdvanceWarsServer::AdvanceWarsServer() :
	m_io_context(),
	m_http_server(m_io_context) {
	std::string app_name("Advance Wars AI");
	unsigned short port_number(via::comms::tcp_adaptor::DEFAULT_HTTP_PORT);
	std::cout << app_name << ": " << port_number << std::endl;
	m_http_server.request_router().add_method("POST", "/games", &AdvanceWarsServer::post_games_handler);
	m_http_server.request_router().add_method("GET", "/actions/:gameid/:x/:y", &AdvanceWarsServer::get_game_actions);
}


int AdvanceWarsServer::run() {
	// Accept connections (both IPV4 and IPV6) on the default port (80)
	asio::error_code error(m_http_server.accept_connections());
	if (error) {
		std::cerr << "Error: " << error.message() << std::endl;
		return -1;
	}
	m_io_context.run();

	return 0;
}

json AdvanceWarsServer::create_new_game(std::string guid) {
	Player player1(CommandingOfficier::Type::Andy, Player::ArmyType::OrangeStar);
	Player player2(CommandingOfficier::Type::Andy, Player::ArmyType::BlueMoon);
	std::array<Player, 2> arrPlayers{ std::move(player1), std::move(player2) };
	GameState* gameState = new GameState(guid, std::move(arrPlayers));
	gameState->InitializeGame();
	m_gameCache.emplace(guid, gameState);
	const auto& game = m_gameCache.find(guid.c_str());
	json j;
	to_json(j, *game->second);
	return j;
}

json AdvanceWarsServer::get_actions(const std::string& gameId, int x, int y) const {
	const auto& game = m_gameCache.find(gameId.c_str());
	std::vector<Action> vecActions;
	game->second->GetValidActions(x, y, vecActions);

	json j;
	to_json(j, vecActions);
	return j;
}

void AdvanceWarsServer::do_action(const std::string& gameId, int x, int y, const Action& action) {
	const auto& game = m_gameCache.find(gameId.c_str());
	game->second->DoAction(x, y, action);
}