import { describe, expect, it } from "vitest";
import { terrainDisplayName } from "./terrain";

describe("terrainDisplayName", () => {
  it("formats terrain names with their numeric IDs", () => {
    expect(terrainDisplayName(1)).toBe("Plain (1)");
    expect(terrainDisplayName(15)).toBe("Road (15)");
    expect(terrainDisplayName(42)).toBe("Orange Star HQ (42)");
  });

  it("keeps unknown terrain IDs inspectable", () => {
    expect(terrainDisplayName(999)).toBe("Unknown (999)");
  });
});
