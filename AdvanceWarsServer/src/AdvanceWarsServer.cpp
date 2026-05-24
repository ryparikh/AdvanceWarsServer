#include "AdvanceWarsServer.h"

#include "via/http/request_uri.hpp"

namespace {
void AddCorsHeaders(tx_response& response) {
	response.add_header("Access-Control-Allow-Origin", "*");
	response.add_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
	response.add_header("Access-Control-Allow-Headers", "Content-Type");
}

tx_response ToHttpResponse(const ApiResponse& apiResponse, std::string& responseBody) {
	responseBody = apiResponse.body.dump();

	tx_response response = apiResponse.status == 422
		? tx_response("Unprocessable Entity", apiResponse.status)
		: tx_response(static_cast<response_status::code>(apiResponse.status));

	for (const auto& [name, value] : apiResponse.headers) {
		response.add_header(name, value);
	}
	AddCorsHeaders(response);
	return response;
}

std::string QueryFromRequest(const AdvanceWarsServer::http_request& request) {
	request_uri uri(request.uri());
	return uri.query();
}
}

std::unique_ptr<AdvanceWarsServer> AdvanceWarsServer::s_spServer{ nullptr };

/*static*/ AdvanceWarsServer& AdvanceWarsServer::getInstance() {
	if (s_spServer == nullptr) {
		s_spServer.reset(new AdvanceWarsServer());
	}

	return *s_spServer;
}

/*static*/ tx_response AdvanceWarsServer::post_games_handler(const http_request& request, const Parameters& parameters, const std::string& data, std::string& response_body) {
	AdvanceWarsServer& server = AdvanceWarsServer::getInstance();
	return ToHttpResponse(server.m_gameService.CreateGame(data), response_body);
}

/*static*/ tx_response AdvanceWarsServer::get_game_handler(const http_request& request, const Parameters& parameters, const std::string& data, std::string& response_body) {
	std::string gameId = parameters.find("gameid")->second;
	AdvanceWarsServer& server = AdvanceWarsServer::getInstance();
	return ToHttpResponse(server.m_gameService.GetGame(gameId), response_body);
}

/*static*/ tx_response AdvanceWarsServer::post_game_actions(const http_request& request, const Parameters& parameters, const std::string& data, std::string& response_body) {
	std::string gameId = parameters.find("gameid")->second;
	AdvanceWarsServer& server = AdvanceWarsServer::getInstance();
	return ToHttpResponse(server.m_gameService.SubmitAction(gameId, data), response_body);
}

/*static*/ tx_response AdvanceWarsServer::get_valid_game_actions(const http_request& request, const Parameters& parameters, const std::string& data, std::string& response_body) {
	std::string gameId = parameters.find("gameid")->second;
	AdvanceWarsServer& server = AdvanceWarsServer::getInstance();
	return ToHttpResponse(server.m_gameService.ListActions(gameId, QueryFromRequest(request)), response_body);
}

/*static*/ tx_response AdvanceWarsServer::options_handler(const http_request& request, const Parameters& parameters, const std::string& data, std::string& response_body) {
	response_body.clear();
	tx_response response(response_status::code::NO_CONTENT);
	AddCorsHeaders(response);
	return response;
}

AdvanceWarsServer::AdvanceWarsServer() :
	m_io_context(),
	m_http_server(m_io_context) {
	std::string app_name("Advance Wars AI");
	unsigned short port_number(via::comms::tcp_adaptor::DEFAULT_HTTP_PORT);
	std::cout << app_name << ": " << port_number << std::endl;

	m_http_server.request_router().add_method("POST", "/games", &AdvanceWarsServer::post_games_handler);
	m_http_server.request_router().add_method("OPTIONS", "/games", &AdvanceWarsServer::options_handler);

	m_http_server.request_router().add_method("GET", "/games/:gameid", &AdvanceWarsServer::get_game_handler);
	m_http_server.request_router().add_method("OPTIONS", "/games/:gameid", &AdvanceWarsServer::options_handler);

	m_http_server.request_router().add_method("GET", "/games/:gameid/actions", &AdvanceWarsServer::get_valid_game_actions);
	m_http_server.request_router().add_method("POST", "/games/:gameid/actions", &AdvanceWarsServer::post_game_actions);
	m_http_server.request_router().add_method("OPTIONS", "/games/:gameid/actions", &AdvanceWarsServer::options_handler);
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
