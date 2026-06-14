import { describe, expect, it } from "vitest";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { changedTilesBetween, parseVisualizerTracePayload, stateForTraceIndex, stepForTraceIndex } from "./trace";

const initialState = {
  activePlayer: 0,
  "cap-limit": 21,
  "game-over": false,
  gameId: "trace-sample",
  map: [
    [
      {
        terrain: 15,
        unit: {
          ammo: 9,
          fuel: 99,
          health: 100,
          hidden: false,
          moved: false,
          owner: "orange-star",
          type: "infantry"
        }
      },
      {
        terrain: 15
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

const resultingState = {
  ...initialState,
  activePlayer: 1,
  map: [
    [
      {
        terrain: 15
      },
      {
        terrain: 15,
        unit: {
          ammo: 9,
          fuel: 98,
          health: 100,
          hidden: false,
          moved: true,
          owner: "orange-star",
          type: "infantry"
        }
      }
    ]
  ]
};

describe("parseVisualizerTracePayload", () => {
  it("parses the bundled self-play trace sample used by the UI", () => {
    const payload = JSON.parse(readFileSync(resolve("public", "samples", "tiny-self-play-trace.json"), "utf8"));
    const trace = parseVisualizerTracePayload(payload);

    expect(trace.steps.length).toBeGreaterThan(0);
    expect(trace.states).toHaveLength(trace.steps.length + 1);
    expect(stepForTraceIndex(trace, trace.steps.length)?.resultingState).toEqual(trace.finalState);
  });

  it("parses a materialized replay trace into a scrub timeline", () => {
    const trace = parseVisualizerTracePayload({
      traceFormatVersion: "standard-gl-visualizer-trace-v1",
      source: {
        kind: "self-play-replay",
        gameIndex: 0,
        mapId: "mcts"
      },
      initialState,
      steps: [
        {
          ply: 0,
          player: 0,
          actionIndex: 123,
          action: { type: "move-wait", source: [0, 0], target: [1, 0] },
          legalActionCount: 4,
          selectedActionIndex: 123,
          stateTensorChecksum: "0123456789abcdef",
          resultingState
        }
      ],
      terminalReason: "action-limit",
      winner: null
    });

    expect(trace.states).toHaveLength(2);
    expect(stateForTraceIndex(trace, 0).activePlayer).toBe(0);
    expect(stateForTraceIndex(trace, 1).activePlayer).toBe(1);
    expect(stepForTraceIndex(trace, 1)?.action.type).toBe("move-wait");
    expect(stepForTraceIndex(trace, 1)?.legalActionCount).toBe(4);
    expect(trace.terminalReason).toBe("action-limit");
    expect(trace.winner).toBeNull();
  });

  it("reports tile changes between adjacent trace states", () => {
    expect(changedTilesBetween(initialState, resultingState)).toEqual([
      [0, 0],
      [1, 0]
    ]);
  });
});
