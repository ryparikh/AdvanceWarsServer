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
	std::string guidString;
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

/*static*/ tx_response AdvanceWarsServer::post_game_actions(const http_request& request, const Parameters& parameters, const std::string&data, std::string&response_body) {
	std::string gameId = parameters.find("gameid")->second;
	AdvanceWarsServer& server = AdvanceWarsServer::getInstance();

	Action action;
	std::istringstream stream(data);
	json j;
	stream >> j;
	from_json(j, action);

	const json jsonResponseBody = server.do_action(gameId, action);
	response_body = jsonResponseBody.dump();
	tx_response resp(response_status::code::OK);
	resp.add_header("Access-Control-Allow-Origin", "*");
	return resp;
}

/*static*/ tx_response AdvanceWarsServer::get_valid_game_actions(const http_request& request, const Parameters& parameters, const std::string&data, std::string&response_body) {
	std::string gameId = parameters.find("gameid")->second;
	AdvanceWarsServer& server = AdvanceWarsServer::getInstance();
	json j = server.get_valid_actions(gameId);
	response_body = j.dump();

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
	m_http_server.request_router().add_method("GET", "/actions/:gameid", &AdvanceWarsServer::get_valid_game_actions);
	m_http_server.request_router().add_method("POST", "/actions/:gameid", &AdvanceWarsServer::post_game_actions);
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

json AdvanceWarsServer::create_new_game(std::string& gameId) {
	GUID guid;
	HRESULT hCreateGuid = CoCreateGuid(&guid);
	if (hCreateGuid != S_OK) {
		std::cerr << "Failed to create GUID" << std::endl;
		return "";
	}
	// Convert GUID to a string
	RPC_CSTR szUuid = NULL;
	if (::UuidToStringA(&guid, &szUuid) == RPC_S_OK) {
		gameId = (char*)szUuid;
		::RpcStringFreeA(&szUuid);
	}

	Player player1(CommandingOfficier::Type::Andy, Player::ArmyType::OrangeStar);
	Player player2(CommandingOfficier::Type::Adder, Player::ArmyType::BlueMoon);
	std::array<Player, 2> arrPlayers{ std::move(player1), std::move(player2) };
	GameState* gameState = new GameState(gameId, std::move(arrPlayers));
	gameState->InitializeGame();
	m_gameCache.emplace(gameId, gameState);
	const auto& game = m_gameCache.find(gameId.c_str());
	json j;
	GameState::to_json(j, *game->second);
	return j;
}

GameState AdvanceWarsServer::CloneGameState(const std::string& gameId) const {
	const auto& game = m_gameCache.find(gameId.c_str());
	return game->second->Clone();
}

json AdvanceWarsServer::get_actions(const std::string& gameId, int x, int y) const {
	const auto& game = m_gameCache.find(gameId.c_str());
	std::vector<Action> vecActions;
	game->second->GetValidActions(x, y, vecActions);

	json j;
	to_json(j, vecActions);
	return j;
}

json AdvanceWarsServer::get_valid_actions(const std::string& gameId) const {
	std::vector<Action> vecActions;
	get_valid_actions(gameId, vecActions);
	json j;
	to_json(j, vecActions);
	return j;
}

Result AdvanceWarsServer::get_valid_actions(const std::string& gameId, std::vector<Action>& vecActions) const {
	const auto& game = m_gameCache.find(gameId.c_str());
	game->second->GetValidActions(vecActions);
	return Result::Succeeded;
}

json AdvanceWarsServer::do_action(const std::string& gameId, const Action& action) {
	const auto& game = m_gameCache.find(gameId.c_str());
	game->second->DoAction(action);
	std::vector<Action> vecActions;
	// Only action left is EndTurn;
	if (!game->second->FGameOver() && game->second->GetValidActions(vecActions) == Result::Succeeded && vecActions.size() == 1) {
		game->second->EndTurn();
		game->second->CheckPlayerResigns();
	}

	json j;
	GameState::to_json(j, *game->second);
	return j;
}

bool AdvanceWarsServer::game_over(const std::string& gameId) {
	const auto& game = m_gameCache.find(gameId.c_str());
	return game->second->FGameOver();
}
