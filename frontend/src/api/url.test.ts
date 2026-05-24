import { describe, expect, it } from "vitest";
import { normalizeServerBaseUrl, serverApiUrl } from "./url";

describe("normalizeServerBaseUrl", () => {
  it("accepts http and https base URLs and trims trailing slashes", () => {
    expect(normalizeServerBaseUrl(" http://localhost:80/// ")).toBe("http://localhost:80");
    expect(normalizeServerBaseUrl("https://example.test/api/")).toBe("https://example.test/api");
  });

  it("rejects unsupported protocols and credential-bearing URLs", () => {
    expect(() => normalizeServerBaseUrl("file:///tmp/state.json")).toThrow(/http/);
    expect(() => normalizeServerBaseUrl("http://user:pass@example.test")).toThrow(/credentials/);
  });
});

describe("serverApiUrl", () => {
  it("encodes dynamic path segments instead of interpolating them", () => {
    expect(serverApiUrl("http://localhost:8080/api", "actions", "game/../id", 1, 2)).toBe(
      "http://localhost:8080/api/actions/game%2F..%2Fid/1/2"
    );
  });
});
