import { describe, expect, it } from "vitest";
import { parseGameStateFileText } from "./filePayload";

const gameState = {
  activePlayer: 0,
  "cap-limit": 21,
  "game-over": false,
  gameId: "sample",
  map: [
    [
      {
        terrain: 15,
        property: {
          "capture-points": 20,
          owner: "orange-star"
        }
      }
    ]
  ],
  players: [
    {
      armyType: "orange-star",
      co: "Andy",
      funds: 1000,
      "power-meter": {
        charge: 0,
        "cop-stars": 3,
        "scop-stars": 3,
        "star-value": 9000
      },
      "power-status": 0,
      "luck-policy": 1
    },
    {
      armyType: "blue-moon",
      co: "Adder",
      funds: 0,
      "power-meter": {
        charge: 0,
        "cop-stars": 2,
        "scop-stars": 3,
        "star-value": 9000
      },
      "power-status": 0,
      "luck-policy": 2
    }
  ],
  "turn-count": 1,
  "unit-cap": 50,
  winner: -1
};

describe("parseGameStateFileText", () => {
  it("parses a local GameState JSON file", () => {
    const parsed = parseGameStateFileText(JSON.stringify(gameState), "TinyCapture5x5.json");

    expect(parsed.gameState.gameId).toBe("sample");
    expect(parsed.legalActionGroups).toEqual([]);
  });

  it("parses a fixture JSON file and preserves legal action groups", () => {
    const parsed = parseGameStateFileText(
      JSON.stringify({
        "initial-game-state": gameState,
        validActions: [
          {
            source: [0, 0],
            expected: [{ type: "wait", source: [0, 0], target: [0, 0] }]
          }
        ]
      }),
      "fixture.json"
    );

    expect(parsed.gameState.gameId).toBe("sample");
    expect(parsed.legalActionGroups).toHaveLength(1);
  });

  it("parses the first game record from a replay JSONL file", () => {
    const replayText = [
      JSON.stringify({ recordType: "header", replayFormatVersion: 1 }),
      JSON.stringify({ recordType: "game", initialState: gameState, finalState: { ...gameState, gameId: "final" } })
    ].join("\n");

    const parsed = parseGameStateFileText(replayText, "shard.jsonl");

    expect(parsed.gameState.gameId).toBe("sample");
  });

  it("reports invalid JSON with the selected file name", () => {
    expect(() => parseGameStateFileText("{", "bad.json")).toThrow(/bad\.json/);
  });

  it("rejects replay JSONL files without a game record", () => {
    expect(() => parseGameStateFileText(JSON.stringify({ recordType: "header" }), "empty.jsonl")).toThrow(/game record/);
  });
});
