import { describe, expect, it } from "vitest";

import { parseGameState } from "./game-state";

const basicWireGameState = {
  gameId: "sample-game",
  activePlayer: 0,
  "cap-limit": 21,
  "game-over": false,
  map: [
    [
      {
        terrain: 35,
        property: { "capture-points": 20, owner: "orange-star" },
        unit: {
          ammo: 0,
          fuel: 99,
          health: 62,
          "display-health": 7,
          hidden: false,
          moved: true,
          owner: "orange-star",
          type: "infantry",
        },
      },
      {
        terrain: 1,
        unit: {
          ammo: 6,
          fuel: 60,
          health: 100,
          "display-health": 10,
          hidden: false,
          moved: false,
          owner: "blue-moon",
          type: "cruiser",
        },
      },
    ],
    [
      { terrain: 1 },
      {
        terrain: 39,
        property: { "capture-points": 20, owner: "neutral" },
      },
    ],
  ],
  players: [
    {
      armyType: "orange-star",
      co: "Andy",
      funds: 1000,
      "luck-policy": 1,
      "power-meter": {
        charge: 0,
        "cop-stars": 3,
        "scop-stars": 3,
        "star-value": 9000,
      },
      "power-status": 0,
    },
    {
      armyType: "blue-moon",
      co: "Adder",
      funds: 2000,
      "luck-policy": 2,
      "power-meter": {
        charge: 1000,
        "cop-stars": 2,
        "scop-stars": 3,
        "star-value": 9000,
      },
      "power-status": 0,
    },
  ],
  "turn-count": 3,
  "unit-cap": 50,
  winner: -1,
};

describe("parseGameState", () => {
  it("normalizes current wire game state into the domain model", () => {
    const result = parseGameState(basicWireGameState);

    expect(result.ok).toBe(true);
    if (!result.ok) {
      return;
    }

    expect(result.data).toMatchObject({
      gameId: "sample-game",
      activePlayerId: 0,
      gameOver: false,
      settings: { captureLimit: 21, unitCap: 50 },
      turnCount: 3,
      weather: "clear",
      winnerPlayerId: null,
    });
    expect(result.data.players).toEqual([
      expect.objectContaining({
        id: 0,
        armyType: "orange-star",
        co: "Andy",
        funds: 1000,
        luckPolicy: "lowest",
      }),
      expect.objectContaining({
        id: 1,
        armyType: "blue-moon",
        co: "Adder",
        funds: 2000,
        luckPolicy: "highest",
      }),
    ]);
    expect(result.data.board.width).toBe(2);
    expect(result.data.board.height).toBe(2);
    expect(result.data.board.tiles[0]?.[0]).toEqual(
      expect.objectContaining({
        x: 0,
        y: 0,
        terrainId: 35,
        property: {
          capturePoints: 20,
          owner: { kind: "player", playerId: 0 },
        },
        unit: expect.objectContaining({
          type: "infantry",
          ownerPlayerId: 0,
          health: 62,
          displayHealth: 7,
          ammo: 0,
          fuel: 99,
          moved: true,
          hidden: false,
        }),
      }),
    );
    expect(result.data.board.tiles[1]?.[1]?.property?.owner).toEqual({
      kind: "neutral",
    });
  });

  it("rejects maps with ragged rows", () => {
    const result = parseGameState({
      ...basicWireGameState,
      map: [[{ terrain: 1 }], [{ terrain: 1 }, { terrain: 1 }]],
    });

    expect(result.ok).toBe(false);
    if (result.ok) {
      return;
    }
    expect(result.error.code).toBe("invalid-map-shape");
  });

  it("rejects unit owners that do not match a player army", () => {
    const result = parseGameState({
      ...basicWireGameState,
      map: [
        [
          {
            terrain: 1,
            unit: {
              ammo: 0,
              fuel: 99,
              health: 100,
              "display-health": 10,
              hidden: false,
              moved: false,
              owner: "green-earth",
              type: "infantry",
            },
          },
        ],
      ],
    });

    expect(result.ok).toBe(false);
  });

  it("requires server-owned display health", () => {
    const result = parseGameState({
      ...basicWireGameState,
      map: [
        [
          {
            terrain: 1,
            unit: {
              ammo: 0,
              fuel: 99,
              health: 100,
              hidden: false,
              moved: false,
              owner: "orange-star",
              type: "infantry",
            },
          },
        ],
      ],
    });

    expect(result.ok).toBe(false);
  });

  it("allows units without exact raw health when display health is present", () => {
    const result = parseGameState({
      ...basicWireGameState,
      map: [
        [
          {
            terrain: 1,
            unit: {
              ammo: 0,
              fuel: 99,
              "display-health": 7,
              hidden: false,
              moved: false,
              owner: "orange-star",
              type: "infantry",
            },
          },
        ],
      ],
    });

    expect(result.ok).toBe(true);
    if (!result.ok) {
      return;
    }
    expect(result.data.board.tiles[0]?.[0]?.unit).toMatchObject({
      displayHealth: 7,
    });
    expect(result.data.board.tiles[0]?.[0]?.unit).not.toHaveProperty("health");
  });

  it("rejects duplicate player army ids because ownership would be ambiguous", () => {
    const result = parseGameState({
      ...basicWireGameState,
      players: [
        basicWireGameState.players[0],
        { ...basicWireGameState.players[1], armyType: "orange-star" },
      ],
    });

    expect(result.ok).toBe(false);
    if (result.ok) {
      return;
    }
    expect(result.error.code).toBe("duplicate-player-army");
  });

  it("rejects negative winner values other than the no-winner sentinel", () => {
    const result = parseGameState({
      ...basicWireGameState,
      winner: -2,
    });

    expect(result.ok).toBe(false);
    if (result.ok) {
      return;
    }
    expect(result.error.code).toBe("invalid-winner");
  });
});
