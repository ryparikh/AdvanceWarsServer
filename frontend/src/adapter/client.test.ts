import { describe, expect, it } from "vitest";

import basicGameState from "../../samples/wire/current/basic-game-state.json";
import legalActions from "../../samples/wire/current/legal-actions.json";
import { createApiClient, type FetchLike } from "./client";

function jsonResponse(body: unknown, init?: { status?: number; ok?: boolean }) {
  const status = init?.status ?? 200;
  const ok = init?.ok ?? (status >= 200 && status < 300);

  return {
    ok,
    status,
    statusText: ok ? "OK" : "Bad Request",
    json: async () => body,
  } as Response;
}

describe("createApiClient", () => {
  it("creates games through the current POST /games endpoint", async () => {
    const calls: Array<{ url: string; init?: RequestInit }> = [];
    const fetch: FetchLike = async (url, init) => {
      calls.push({ url: String(url), init });
      return jsonResponse(basicGameState);
    };

    const client = createApiClient({
      baseUrl: "http://example.test/root/",
      fetch,
    });

    const result = await client.createGame({ seed: 123 });

    expect(result.ok).toBe(true);
    expect(calls).toHaveLength(1);
    expect(calls[0]?.url).toBe("http://example.test/root/games");
    expect(calls[0]?.init?.method).toBe("POST");
    expect(calls[0]?.init?.body).toBe(JSON.stringify({ seed: 123 }));
  });

  it("fetches target game state from GET /games/:gameId", async () => {
    const calls: string[] = [];
    const fetch: FetchLike = async (url) => {
      calls.push(String(url));
      return jsonResponse(basicGameState);
    };

    const client = createApiClient({ baseUrl: "http://example.test", fetch });

    const result = await client.getGameState("abc 123");

    expect(result.ok).toBe(true);
    expect(calls).toEqual(["http://example.test/games/abc%20123"]);
  });

  it("uses current legal-action endpoints with optional source coordinates", async () => {
    const calls: string[] = [];
    const fetch: FetchLike = async (url) => {
      calls.push(String(url));
      return jsonResponse(legalActions);
    };

    const client = createApiClient({ baseUrl: "http://example.test", fetch });

    const allActions = await client.getLegalActions("game-a");
    const tileActions = await client.getLegalActions("game-a", { x: 4, y: 7 });

    expect(allActions.ok).toBe(true);
    expect(tileActions.ok).toBe(true);
    expect(calls).toEqual([
      "http://example.test/actions/game-a",
      "http://example.test/actions/game-a/4/7",
    ]);
  });

  it("serializes submitted domain actions to server wire shape", async () => {
    const calls: Array<{ url: string; init?: RequestInit }> = [];
    const fetch: FetchLike = async (url, init) => {
      calls.push({ url: String(url), init });
      return jsonResponse(basicGameState);
    };

    const client = createApiClient({ baseUrl: "http://example.test", fetch });

    const result = await client.submitAction("game-a", {
      type: "buy",
      source: { x: 4, y: 5 },
      unitType: "cruiser",
    });

    expect(result.ok).toBe(true);
    if (!result.ok) {
      return;
    }
    expect(result.data.action.type).toBe("buy");
    expect(calls[0]?.url).toBe("http://example.test/actions/game-a");
    expect(calls[0]?.init?.method).toBe("POST");
    expect(calls[0]?.init?.body).toBe(
      JSON.stringify({ type: "buy", source: [4, 5], unit: "cruiser" }),
    );
  });

  it("returns ApiResult failures for non-2xx HTTP responses", async () => {
    const fetch: FetchLike = async () =>
      jsonResponse({ message: "Nope" }, { status: 404, ok: false });

    const client = createApiClient({ baseUrl: "http://example.test", fetch });

    const result = await client.getGameState("missing");

    expect(result.ok).toBe(false);
    if (result.ok) {
      return;
    }
    expect(result.error).toMatchObject({
      code: "http-error",
      source: "http",
      status: 404,
    });
  });

  it("returns ApiResult failures for network errors", async () => {
    const fetch: FetchLike = async () => {
      throw new Error("offline");
    };

    const client = createApiClient({ baseUrl: "http://example.test", fetch });

    const result = await client.getLegalActions("game-a");

    expect(result.ok).toBe(false);
    if (result.ok) {
      return;
    }
    expect(result.error).toMatchObject({
      code: "network-error",
      source: "network",
    });
  });

  it("returns ApiResult failures for invalid JSON response bodies", async () => {
    const fetch: FetchLike = async () =>
      ({
        ok: true,
        status: 200,
        statusText: "OK",
        json: async () => {
          throw new SyntaxError("bad json");
        },
      }) as unknown as Response;

    const client = createApiClient({ baseUrl: "http://example.test", fetch });

    const result = await client.getLegalActions("game-a");

    expect(result.ok).toBe(false);
    if (result.ok) {
      return;
    }
    expect(result.error).toMatchObject({
      code: "invalid-json",
      source: "parse",
    });
  });

  it("returns parse failures for malformed game-state payloads", async () => {
    const fetch: FetchLike = async () => jsonResponse({ gameId: "bad" });
    const client = createApiClient({ baseUrl: "http://example.test", fetch });

    const result = await client.createGame({});

    expect(result.ok).toBe(false);
    if (result.ok) {
      return;
    }
    expect(result.error.source).toBe("parse");
  });
});
