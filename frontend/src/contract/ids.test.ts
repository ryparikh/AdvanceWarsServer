import { describe, expect, it } from "vitest";

import {
  ActionTypeSchema,
  CommandingOfficerSchema,
  OwnerSchema,
  UnitTypeSchema,
} from "./ids";

describe("contract id schemas", () => {
  it("accepts the corrected cruiser unit id", () => {
    expect(UnitTypeSchema.parse("cruiser")).toBe("cruiser");
  });

  it("rejects the legacy misspelled cruiser unit id", () => {
    expect(() => UnitTypeSchema.parse("crusier")).toThrow();
  });

  it("accepts current server-owned ids", () => {
    expect(CommandingOfficerSchema.parse("Andy")).toBe("Andy");
    expect(ActionTypeSchema.parse("move-attack")).toBe("move-attack");
    expect(OwnerSchema.parse("neutral")).toBe("neutral");
    expect(OwnerSchema.parse("orange-star")).toBe("orange-star");
  });
});
