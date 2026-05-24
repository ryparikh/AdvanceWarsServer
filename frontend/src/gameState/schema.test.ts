import { describe, expect, it } from "vitest";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { actionListSchema, parseGameStatePayload } from "./schema";

const gameState = {
  activePlayer: 0,
  "cap-limit": 21,
  "game-over": false,
  gameId: "sample",
  map: [
    [
      {
        terrain: 15,
        unit: {
          ammo: 0,
          fuel: 80,
          health: 100,
          hidden: false,
          moved: false,
          owner: "orange-star",
          type: "recon"
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

describe("parseGameStatePayload", () => {
  it("parses the bundled sample payloads used by the UI", () => {
    for (const sample of ["lefty.json", "grit-indirect-range-valid-actions.json"]) {
      const payload = JSON.parse(readFileSync(resolve("public", "samples", sample), "utf8"));
      expect(parseGameStatePayload(payload).gameState.map.length).toBeGreaterThan(0);
    }
  });

  it("parses a raw serialized GameState", () => {
    const parsed = parseGameStatePayload(gameState);

    expect(parsed.gameState.gameId).toBe("sample");
    expect(parsed.legalActionGroups).toEqual([]);
  });

  it("unwraps JSON fixture payloads and keeps valid action groups", () => {
    const parsed = parseGameStatePayload({
      "initial-game-state": gameState,
      validActions: [
        {
          source: [0, 0],
          expected: [
            { type: "move-wait", source: [0, 0], target: [0, 0] },
            { type: "attack", source: [0, 0], target: [2, 0] }
          ]
        }
      ]
    });

    expect(parsed.gameState.gameId).toBe("sample");
    expect(parsed.legalActionGroups).toHaveLength(1);
    expect(parsed.legalActionGroups[0].expected[1].type).toBe("attack");
  });

  it("rejects ragged maps before rendering", () => {
    expect(() =>
      parseGameStatePayload({
        ...gameState,
        map: [
          [{ terrain: 15 }],
          [{ terrain: 15 }, { terrain: 15 }]
        ]
      })
    ).toThrow(/rectangular/);
  });

  it("rejects oversized maps and unbounded text fields", () => {
    const hugeRow = Array.from({ length: 65 }, () => ({ terrain: 15 }));
    expect(() =>
      parseGameStatePayload({
        ...gameState,
        gameId: "x".repeat(81),
        map: [hugeRow]
      })
    ).toThrow();
  });

  it("rejects nested transport cargo", () => {
    expect(() =>
      parseGameStatePayload({
        ...gameState,
        map: [
          [
            {
              terrain: 15,
              unit: {
                ammo: 0,
                fuel: 80,
                health: 100,
                hidden: false,
                moved: false,
                owner: "orange-star",
                type: "lander",
                "loaded-units": [
                  {
                    ammo: 0,
                    fuel: 70,
                    health: 100,
                    hidden: false,
                    moved: false,
                    owner: "orange-star",
                    type: "apc",
                    "loaded-units": [
                      {
                        ammo: 0,
                        fuel: 99,
                        health: 100,
                        hidden: false,
                        moved: false,
                        owner: "orange-star",
                        type: "infantry"
                      }
                    ]
                  }
                ]
              }
            }
          ]
        ]
      })
    ).toThrow(/nested cargo/);
  });
});

describe("actionListSchema", () => {
  it("caps action responses before they can drive large highlight sets", () => {
    const actions = Array.from({ length: 513 }, () => ({ type: "wait", source: [0, 0] }));
    expect(() => actionListSchema.parse(actions)).toThrow();
  });
});
