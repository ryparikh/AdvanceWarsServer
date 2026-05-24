import { describe, expect, it } from "vitest";
import { actionHighlightsForSource } from "./actions";
import { unitAssetName, unitAssetPath } from "./unitAssets";

describe("unitAssetName", () => {
  it("normalizes server unit names to reference asset filenames", () => {
    expect(unitAssetName("anti-air")).toBe("anti-air");
    expect(unitAssetName("bcopter")).toBe("b-copter");
    expect(unitAssetName("tcopter")).toBe("t-copter");
    expect(unitAssetName("medium-tank")).toBe("mdtank");
    expect(unitAssetName("crusier")).toBe("cruiser");
  });

  it("rejects unknown unit names instead of using them as asset paths", () => {
    expect(unitAssetName("../tank")).toBeUndefined();
    expect(unitAssetPath("../orange-star", "tank", false)).toBeUndefined();
    expect(unitAssetPath("orange-star", "../tank", false)).toBeUndefined();
  });
});

describe("actionHighlightsForSource", () => {
  it("maps move actions to blue highlights and attack actions to red highlights", () => {
    const highlights = actionHighlightsForSource(
      [
        { type: "move-wait", source: [1, 1], target: [2, 1] },
        { type: "move-capture", source: [1, 1], target: [1, 2] },
        { type: "attack", source: [1, 1], target: [4, 1] },
        { type: "move-attack", source: [1, 1], target: [3, 1], direction: "east" },
        { type: "end-turn" }
      ],
      [1, 1]
    );

    expect(highlights.moves.map((tile) => tile.key)).toEqual(["2,1", "1,2", "3,1"]);
    expect(highlights.attacks.map((tile) => tile.key)).toEqual(["4,1", "4,1"]);
    expect(highlights.actionsByTile.get("4,1")?.map((action) => action.type)).toEqual([
      "attack",
      "move-attack"
    ]);
  });
});
