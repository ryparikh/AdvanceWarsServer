#include "RestGameService.h"

#include <algorithm>
#include <initializer_list>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "StandardGameSetup.h"

namespace {
constexpr int HttpOk = 200;
constexpr int HttpCreated = 201;
constexpr int HttpBadRequest = 400;
constexpr int HttpNotFound = 404;
constexpr int HttpConflict = 409;
constexpr int HttpUnprocessableEntity = 422;
constexpr int HttpInternalServerError = 500;

ApiResponse JsonResponse(int status, json body) {
	ApiResponse response;
	response.status = status;
	response.body = std::move(body);
	response.headers.emplace("Content-Type", "application/json");
	return response;
}

ApiResponse ErrorResponse(int status, const std::string& code, const std::string& message, json details = json::object()) {
	json body = {
		{ "error", {
			{ "code", code },
			{ "message", message },
		} },
	};
	if (!details.empty()) {
		body["error"]["details"] = std::move(details);
	}
	return JsonResponse(status, std::move(body));
}

ApiResponse ErrorResponseWithGame(int status, const std::string& code, const std::string& message, const json& game) {
	json body = {
		{ "error", {
			{ "code", code },
			{ "message", message },
		} },
		{ "game", game },
	};
	return JsonResponse(status, std::move(body));
}

bool ContainsOnlyFields(const json& object, const std::vector<std::string>& allowedFields, std::string& invalidField) {
	if (!object.is_object()) {
		invalidField.clear();
		return false;
	}

	for (auto it = object.begin(); it != object.end(); ++it) {
		if (std::find(allowedFields.begin(), allowedFields.end(), it.key()) == allowedFields.end()) {
			invalidField = it.key();
			return false;
		}
	}

	return true;
}

void RedactSonjaHiddenHp(json& unit, const std::string& sonjaArmyType) {
	if (!unit.is_object()) {
		return;
	}

	if (unit.contains("owner") && unit.at("owner").is_string() && unit.at("owner").get<std::string>() == sonjaArmyType) {
		unit["health"] = nullptr;
		unit["hidden-health"] = true;
	}

	if (unit.contains("loaded-units") && unit.at("loaded-units").is_array()) {
		for (json& loadedUnit : unit["loaded-units"]) {
			RedactSonjaHiddenHp(loadedUnit, sonjaArmyType);
		}
	}
}

void RedactHiddenHpForPerspective(json& game, int playerIndex) {
	const int opponentIndex = playerIndex == 0 ? 1 : 0;
	const json& opponent = game.at("players").at(opponentIndex);
	if (opponent.at("co") != "Sonja") {
		return;
	}

	const std::string sonjaArmyType = opponent.at("armyType").get<std::string>();
	for (json& row : game["map"]) {
		for (json& tile : row) {
			if (tile.contains("unit")) {
				RedactSonjaHiddenHp(tile["unit"], sonjaArmyType);
			}
		}
	}
}

ApiResponse UnknownGameResponse() {
	return ErrorResponse(HttpNotFound, "unknown-game-id", "Game id was not found.");
}

int HttpStatusFromSetupError(StandardGameSetupErrorType type) {
	switch (type) {
	case StandardGameSetupErrorType::BadRequest:
		return HttpBadRequest;
	case StandardGameSetupErrorType::UnprocessableEntity:
		return HttpUnprocessableEntity;
	case StandardGameSetupErrorType::Internal:
	default:
		return HttpInternalServerError;
	}
}

bool TryGetInteger(const std::string& value, int& parsed) {
	try {
		size_t processed = 0;
		parsed = std::stoi(value, &processed);
		return processed == value.size();
	}
	catch (const std::exception&) {
		return false;
	}
}

bool ContainsQueryField(std::initializer_list<const char*> allowedFields, const std::string& key) {
	for (const char* allowedField : allowedFields) {
		if (key == allowedField) {
			return true;
		}
	}

	return false;
}

std::map<std::string, std::string> ParseQueryString(const std::string& query, std::initializer_list<const char*> allowedFields, bool& valid) {
	valid = true;
	std::map<std::string, std::string> parsed;
	if (query.empty()) {
		return parsed;
	}

	std::stringstream stream(query);
	std::string part;
	while (std::getline(stream, part, '&')) {
		const size_t equals = part.find('=');
		if (equals == std::string::npos) {
			valid = false;
			return parsed;
		}

		std::string key = part.substr(0, equals);
		std::string value = part.substr(equals + 1);
		if (!ContainsQueryField(allowedFields, key) || parsed.find(key) != parsed.end()) {
			valid = false;
			return parsed;
		}

		parsed.emplace(std::move(key), std::move(value));
	}

	return parsed;
}

json LegalActionsEnvelope(const GameState& gameState, const std::string& gameId, const std::vector<Action>& actions) {
	json envelope = {
		{ "gameId", gameId },
		{ "activePlayer", gameState.getCurrentPlayer() },
		{ "actions", actions },
	};
	return envelope;
}

ApiResponse ParseActionBody(const std::string& requestBody, Action& action) {
	json request;
	try {
		request = json::parse(requestBody);
	}
	catch (const std::exception&) {
		return ErrorResponse(HttpBadRequest, "malformed-json", "Request body is not valid JSON.");
	}

	if (!request.is_object()) {
		return ErrorResponse(HttpBadRequest, "invalid-field", "Action payload must be an object.");
	}

	std::string invalidField;
	if (!ContainsOnlyFields(request, { "type", "source", "target", "direction", "unit", "unloadIndex" }, invalidField)) {
		return ErrorResponse(HttpBadRequest, "invalid-field", "Unknown action field.", { { "field", invalidField } });
	}

	if (!request.contains("type") || !request.at("type").is_string()) {
		return ErrorResponse(HttpBadRequest, "invalid-field", "Action type is required.", { { "field", "type" } });
	}

	try {
		from_json(request, action);
	}
	catch (const std::exception&) {
		return ErrorResponse(HttpBadRequest, "invalid-field", "Action payload has an invalid field shape.");
	}

	if (action.m_type == Action::Type::Invalid) {
		return ErrorResponse(HttpBadRequest, "unknown-action-id", "Action type is not recognized.", { { "field", "type" }, { "value", request.at("type") } });
	}

	if (request.contains("unit") && (!action.m_optUnitType.has_value() || action.m_optUnitType.value() == UnitProperties::Type::Invalid)) {
		return ErrorResponse(HttpBadRequest, "unknown-unit-id", "Unit id is not recognized.", { { "field", "unit" }, { "value", request.at("unit") } });
	}

	if (request.contains("direction") && !action.m_optDirection.has_value()) {
		return ErrorResponse(HttpBadRequest, "unknown-direction", "Direction is not recognized.", { { "field", "direction" }, { "value", request.at("direction") } });
	}

	return JsonResponse(HttpOk, json::object());
}
}

RestGameService::GameCache::iterator RestGameService::FindGame(const std::string& gameId) {
	return m_games.find(gameId);
}

RestGameService::GameCache::const_iterator RestGameService::FindGame(const std::string& gameId) const {
	return m_games.find(gameId);
}

ApiResponse RestGameService::CreateGame(const std::string& requestBody) {
	json request;
	try {
		request = json::parse(requestBody.empty() ? "{}" : requestBody);
	}
	catch (const std::exception&) {
		return ErrorResponse(HttpBadRequest, "malformed-json", "Request body is not valid JSON.");
	}

	StandardGameSetupResult setupResult;
	StandardGameSetupError setupError;
	if (CreateStandardGameFromRequest(request, setupResult, setupError) == Result::Failed) {
		return ErrorResponse(HttpStatusFromSetupError(setupError.type), setupError.code, setupError.message, setupError.details);
	}

	const std::string gameId = setupResult.gameState.GetId();
	m_games.emplace(gameId, std::unique_ptr<GameState>(new GameState(setupResult.gameState)));

	ApiResponse response = JsonResponse(HttpCreated, SerializeStandardGameForApi(*m_games.at(gameId)));
	response.headers.emplace("Location", "/games/" + gameId);
	return response;
}

ApiResponse RestGameService::GetGame(const std::string& gameId, const std::string& query) const {
	const auto game = FindGame(gameId);
	if (game == m_games.end()) {
		return UnknownGameResponse();
	}

	bool validQuery = true;
	std::map<std::string, std::string> queryParams = ParseQueryString(query, { "player" }, validQuery);
	if (!validQuery) {
		return ErrorResponse(HttpBadRequest, "invalid-query", "Game query may only include player.");
	}

	int playerIndex = -1;
	if (!queryParams.empty()) {
		if (!TryGetInteger(queryParams["player"], playerIndex) || playerIndex < 0 || playerIndex > 1) {
			return ErrorResponse(HttpUnprocessableEntity, "invalid-player", "Player perspective must be 0 or 1.", { { "field", "player" }, { "value", queryParams["player"] } });
		}
	}

	json response = SerializeStandardGameForApi(*game->second);
	if (playerIndex != -1) {
		RedactHiddenHpForPerspective(response, playerIndex);
	}

	return JsonResponse(HttpOk, std::move(response));
}

ApiResponse RestGameService::ListActions(const std::string& gameId, const std::string& query) const {
	const auto game = FindGame(gameId);
	if (game == m_games.end()) {
		return UnknownGameResponse();
	}

	bool validQuery = true;
	std::map<std::string, std::string> queryParams = ParseQueryString(query, { "x", "y" }, validQuery);
	if (!validQuery || queryParams.size() == 1) {
		return ErrorResponse(HttpBadRequest, "invalid-query", "Legal action query must include both x and y, or neither.");
	}

	std::vector<Action> actions;
	json envelope;
	if (queryParams.empty()) {
		game->second->GetValidActions(actions);
		envelope = LegalActionsEnvelope(*game->second, gameId, actions);
	}
	else {
		int x = 0;
		int y = 0;
		if (!TryGetInteger(queryParams["x"], x) || !TryGetInteger(queryParams["y"], y)) {
			return ErrorResponse(HttpUnprocessableEntity, "invalid-coordinate", "x and y must be integer board coordinates.", { { "x", queryParams["x"] }, { "y", queryParams["y"] } });
		}

		if (game->second->GetValidActions(x, y, actions) == Result::Failed) {
			return ErrorResponse(HttpUnprocessableEntity, "invalid-coordinate", "x and y must be in bounds.", { { "x", x }, { "y", y } });
		}

		envelope = LegalActionsEnvelope(*game->second, gameId, actions);
		envelope["source"] = json::array({ x, y });
	}

	return JsonResponse(HttpOk, std::move(envelope));
}

ApiResponse RestGameService::SubmitAction(const std::string& gameId, const std::string& requestBody) {
	const auto game = FindGame(gameId);
	if (game == m_games.end()) {
		return UnknownGameResponse();
	}

	json currentGame = SerializeStandardGameForApi(*game->second);
	if (game->second->FGameOver()) {
		return ErrorResponseWithGame(HttpConflict, "terminal-state", "Cannot apply an action to a terminal game.", currentGame);
	}

	Action action;
	ApiResponse actionParse = ParseActionBody(requestBody, action);
	if (actionParse.status != HttpOk) {
		return actionParse;
	}

	std::vector<Action> legalActions;
	game->second->GetValidActions(legalActions);
	if (std::find(legalActions.begin(), legalActions.end(), action) == legalActions.end()) {
		return ErrorResponseWithGame(HttpUnprocessableEntity, "illegal-action", "Action is not legal for the current player.", currentGame);
	}

	if (game->second->DoAction(action) == Result::Failed) {
		return ErrorResponseWithGame(HttpUnprocessableEntity, "illegal-action", "Action failed during execution.", currentGame);
	}

	if (game->second->FHeuristicAutoResign()) {
		game->second->CheckPlayerResigns();
	}

	return JsonResponse(HttpOk, SerializeStandardGameForApi(*game->second));
}

void RestGameService::StoreGameForTesting(GameState gameState) {
	const std::string gameId = gameState.GetId();
	m_games[gameId] = std::unique_ptr<GameState>(new GameState(gameState));
}
