const supportedProtocols = new Set(["http:", "https:"]);

export function normalizeServerBaseUrl(value: string): string {
  const trimmed = value.trim().replace(/\/+$/, "");
  if (!trimmed) {
    throw new Error("Server URL is required");
  }

  let parsed: URL;
  try {
    parsed = new URL(trimmed);
  } catch {
    throw new Error("Server URL must include http:// or https://");
  }

  if (!supportedProtocols.has(parsed.protocol)) {
    throw new Error("Server URL must use http:// or https://");
  }

  if (parsed.username || parsed.password) {
    throw new Error("Server URL cannot include credentials");
  }

  if (parsed.search || parsed.hash) {
    throw new Error("Server URL cannot include query strings or fragments");
  }

  return trimmed;
}

export function serverApiUrl(baseUrl: string, ...pathSegments: Array<string | number>): string {
  const normalizedBase = normalizeServerBaseUrl(baseUrl);
  const url = new URL(normalizedBase.endsWith("/") ? normalizedBase : `${normalizedBase}/`);
  const baseSegments = url.pathname.split("/").filter(Boolean);
  const encodedSegments = pathSegments.map((segment) => encodeURIComponent(String(segment)));
  url.pathname = `/${[...baseSegments, ...encodedSegments].join("/")}`;
  return url.toString();
}
