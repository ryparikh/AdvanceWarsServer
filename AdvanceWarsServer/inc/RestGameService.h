#pragma once

#include <map>
#include <memory>
#include <string>

#include "GameState.h"

struct ApiResponse {
	int status{ 200 };
	json body;
	std::map<std::string, std::string> headers;
};

class RestGameService {
public:
	ApiResponse CreateGame(const std::string& requestBody);
	ApiResponse GetGame(const std::string& gameId, const std::string& query = "") const;
	ApiResponse ListActions(const std::string& gameId, const std::string& query) const;
	ApiResponse SubmitAction(const std::string& gameId, const std::string& requestBody);

	void StoreGameForTesting(GameState gameState);

private:
	using GameCache = std::map<std::string, std::unique_ptr<GameState>>;

	GameCache::iterator FindGame(const std::string& gameId);
	GameCache::const_iterator FindGame(const std::string& gameId) const;

	GameCache m_games;
};
