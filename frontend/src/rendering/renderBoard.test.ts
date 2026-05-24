import { describe, expect, it } from "vitest";
import { boardTileSize, routeStrokeForPath } from "./renderBoard";

describe("routeStrokeForPath", () => {
  it("backs the line endpoint away from the arrow tip", () => {
    const center = boardTileSize / 2;

    expect(routeStrokeForPath([[0, 0], [2, 0]], true)).toEqual({
      linePoints: [
        { x: center, y: center },
        { x: center + boardTileSize * 2 - 6, y: center }
      ],
      arrowFrom: { x: center, y: center },
      arrowTo: { x: center + boardTileSize * 2, y: center }
    });
  });
});
