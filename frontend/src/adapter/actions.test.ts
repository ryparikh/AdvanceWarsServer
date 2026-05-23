import { describe, expect, it } from "vitest";

import { parseActions, serializeAction } from "./actions";

describe("action adapter", () => {
  it("normalizes wire action coordinates to objects", () => {
    const result = parseActions([
      {
        type: "move-attack",
        source: [0, 1],
        target: [2, 3],
        direction: "east",
      },
      { type: "end-turn" },
    ]);

    expect(result.ok).toBe(true);
    if (!result.ok) {
      return;
    }

    expect(result.data).toEqual([
      {
        type: "move-attack",
        source: { x: 0, y: 1 },
        target: { x: 2, y: 3 },
        direction: "east",
      },
      { type: "end-turn" },
    ]);
  });

  it("serializes domain actions back to exact wire action shape", () => {
    expect(
      serializeAction({
        type: "buy",
        source: { x: 4, y: 5 },
        unitType: "cruiser",
      }),
    ).toEqual({
      type: "buy",
      source: [4, 5],
      unit: "cruiser",
    });
  });
});
