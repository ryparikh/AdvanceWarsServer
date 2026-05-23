import type { Action, Coordinate, GameState } from "../domain/types";
import { parseActions } from "./actions";
import { parseGameState } from "./game-state";
import { fail, ok, type ApiError, type ApiResult } from "./result";
import { serializeAction } from "./actions";

export type FetchLike = (
  input: string | URL | Request,
  init?: RequestInit,
) => Promise<Response>;

export type CreateGameRequest = {
  mapId?: string;
  seed?: number;
  players?: Array<{
    co?: string;
    armyType?: string;
  }>;
  settings?: Record<string, unknown>;
};

export type SubmitActionData = {
  action: Action;
  state: GameState;
};

export type ApiClient = {
  createGame(request?: CreateGameRequest): Promise<ApiResult<GameState>>;
  getGameState(gameId: string): Promise<ApiResult<GameState>>;
  getLegalActions(
    gameId: string,
    source?: Coordinate,
  ): Promise<ApiResult<Action[]>>;
  submitAction(
    gameId: string,
    action: Action,
  ): Promise<ApiResult<SubmitActionData>>;
};

export type ApiClientOptions = {
  baseUrl?: string;
  fetch?: FetchLike;
};

function defaultFetch(): FetchLike {
  if (typeof fetch !== "function") {
    throw new Error("No global fetch implementation is available.");
  }

  return fetch;
}

function normalizeBaseUrl(baseUrl: string): string {
  return baseUrl.replace(/\/+$/, "");
}

function encodePathSegment(value: string): string {
  return encodeURIComponent(value);
}

function makeUrl(baseUrl: string, path: string): string {
  return `${baseUrl}${path}`;
}

async function readJson(response: Response): Promise<ApiResult<unknown>> {
  try {
    return ok(await response.json());
  } catch (error) {
    return fail({
      code: "invalid-json",
      message: "Response body was not valid JSON.",
      source: "parse",
      details: error,
    });
  }
}

async function requestJson(
  fetchImpl: FetchLike,
  url: string,
  init?: RequestInit,
): Promise<ApiResult<unknown>> {
  let response: Response;
  try {
    response = await fetchImpl(url, init);
  } catch (error) {
    return fail({
      code: "network-error",
      message: "Network request failed.",
      source: "network",
      details: error,
    });
  }

  const body = await readJson(response);

  if (!response.ok) {
    const error: ApiError = {
      code: "http-error",
      message: response.statusText || `HTTP ${response.status}`,
      source: "http",
      status: response.status,
      ...(body.ok ? { details: body.data } : { details: body.error.details }),
    };
    return fail(error);
  }

  return body;
}

function jsonInit(method: "POST", body: unknown): RequestInit {
  return {
    method,
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify(body),
  };
}

export function createApiClient(options: ApiClientOptions = {}): ApiClient {
  const baseUrl = normalizeBaseUrl(options.baseUrl ?? "http://localhost:80");
  const fetchImpl = options.fetch ?? defaultFetch();

  return {
    async createGame(request: CreateGameRequest = {}) {
      const body = await requestJson(
        fetchImpl,
        makeUrl(baseUrl, "/games"),
        jsonInit("POST", request),
      );
      if (!body.ok) {
        return body;
      }

      return parseGameState(body.data);
    },

    async getGameState(gameId: string) {
      const body = await requestJson(
        fetchImpl,
        makeUrl(baseUrl, `/games/${encodePathSegment(gameId)}`),
      );
      if (!body.ok) {
        return body;
      }

      return parseGameState(body.data);
    },

    async getLegalActions(gameId: string, source?: Coordinate) {
      const encodedGameId = encodePathSegment(gameId);
      const path = source
        ? `/actions/${encodedGameId}/${source.x}/${source.y}`
        : `/actions/${encodedGameId}`;
      const body = await requestJson(fetchImpl, makeUrl(baseUrl, path));
      if (!body.ok) {
        return body;
      }

      return parseActions(body.data);
    },

    async submitAction(gameId: string, action: Action) {
      const body = await requestJson(
        fetchImpl,
        makeUrl(baseUrl, `/actions/${encodePathSegment(gameId)}`),
        jsonInit("POST", serializeAction(action)),
      );
      if (!body.ok) {
        return body;
      }

      const state = parseGameState(body.data);
      if (!state.ok) {
        return state;
      }

      return ok({ action, state: state.data });
    },
  };
}
