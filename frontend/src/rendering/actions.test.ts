import { describe, expect, it } from "vitest";
import { actionHighlightsForSource, actionPreviewForTile, movementTrailAfterHover, orthogonalPath } from "./actions";
import { unitAssetName, unitAssetPath } from "./unitAssets";
import type { Coordinate } from "../gameState/schema";

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

describe("orthogonalPath", () => {
  it("builds a board-space movement route with one bend", () => {
    expect(orthogonalPath([1, 4], [4, 2])).toEqual([
      [1, 4],
      [1, 2],
      [4, 2]
    ]);
  });

  it("omits duplicate bend points for straight movement", () => {
    expect(orthogonalPath([1, 4], [1, 2])).toEqual([
      [1, 4],
      [1, 2]
    ]);
  });
});

describe("actionPreviewForTile", () => {
  it("uses the cursor movement trail for move targets", () => {
    const trail: Coordinate[] = [
      [1, 4],
      [2, 4],
      [3, 4],
      [3, 3],
      [4, 3],
      [4, 2]
    ];

    expect(
      actionPreviewForTile(
        [{ type: "move-wait", source: [1, 4], target: [4, 2] }],
        [1, 4],
        [4, 2],
        [...trail]
      )
    ).toEqual({
      kind: "move",
      path: [...trail]
    });
  });

  it("prefers a movement route for move targets", () => {
    expect(
      actionPreviewForTile(
        [{ type: "move-wait", source: [1, 4], target: [4, 2] }],
        [1, 4],
        [4, 2]
      )
    ).toEqual({
      kind: "move",
      path: [
        [1, 4],
        [1, 2],
        [4, 2]
      ]
    });
  });

  it("creates an attack preview for direct attacks", () => {
    expect(
      actionPreviewForTile(
        [{ type: "attack", source: [3, 2], target: [4, 2] }],
        [3, 2],
        [4, 2]
      )
    ).toEqual({
      kind: "attack",
      attacker: [3, 2],
      attackerUnit: [3, 2],
      defender: [4, 2],
      route: undefined
    });
  });

  it("creates a moved attack preview from the move destination", () => {
    expect(
      actionPreviewForTile(
        [{ type: "move-attack", source: [1, 4], target: [3, 2], direction: "east" }],
        [1, 4],
        [4, 2]
      )
    ).toEqual({
      kind: "attack",
      attacker: [3, 2],
      attackerUnit: [1, 4],
      defender: [4, 2],
      route: [
        [1, 4],
        [1, 2],
        [3, 2]
      ]
    });
  });

  it("uses the cursor movement trail for moved attacks", () => {
    const trail: Coordinate[] = [
      [1, 4],
      [2, 4],
      [3, 4],
      [3, 3],
      [3, 2]
    ];

    expect(
      actionPreviewForTile(
        [{ type: "move-attack", source: [1, 4], target: [3, 2], direction: "east" }],
        [1, 4],
        [4, 2],
        [...trail]
      )
    ).toEqual({
      kind: "attack",
      attacker: [3, 2],
      attackerUnit: [1, 4],
      defender: [4, 2],
      route: [...trail]
    });
  });
});

describe("movementTrailAfterHover", () => {
  const source: Coordinate = [0, 0];
  const highlights = actionHighlightsForSource(
    [
      { type: "move-wait", source: [0, 0], target: [1, 0] },
      { type: "move-wait", source: [0, 0], target: [2, 0] },
      { type: "move-wait", source: [0, 0], target: [2, 1] },
      { type: "move-wait", source: [0, 0], target: [2, 2] },
      { type: "attack", source: [0, 0], target: [3, 0] }
    ],
    [0, 0]
  );

  it("extends through adjacent valid movement hovers", () => {
    let trail = movementTrailAfterHover(undefined, [...source], [1, 0], highlights);
    trail = movementTrailAfterHover(trail, [...source], [2, 0], highlights);
    trail = movementTrailAfterHover(trail, [...source], [2, 1], highlights);

    expect(trail).toEqual([
      [0, 0],
      [1, 0],
      [2, 0],
      [2, 1]
    ]);
  });

  it("truncates when hovering an earlier point in the trail", () => {
    expect(
      movementTrailAfterHover(
        [
          [0, 0],
          [1, 0],
          [2, 0],
          [2, 1]
        ],
        [...source],
        [1, 0],
        highlights
      )
    ).toEqual([
      [0, 0],
      [1, 0]
    ]);
  });

  it("keeps the route while hovering non-move targets", () => {
    const trail: Coordinate[] = [
      [0, 0],
      [1, 0],
      [2, 0]
    ];

    expect(movementTrailAfterHover(trail, [...source], [3, 0], highlights)).toBe(trail);
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
