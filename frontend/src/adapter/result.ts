import { ZodError } from "zod";

export type ApiErrorSource =
  | "network"
  | "http"
  | "parse"
  | "server"
  | "validation";

export type ApiError = {
  code: string;
  message: string;
  source: ApiErrorSource;
  status?: number;
  path?: string;
  details?: unknown;
};

export type ApiResult<T> =
  | { ok: true; data: T }
  | { ok: false; error: ApiError };

export function ok<T>(data: T): ApiResult<T> {
  return { ok: true, data };
}

export function fail<T = never>(error: ApiError): ApiResult<T> {
  return { ok: false, error };
}

export function zodParseError(error: ZodError): ApiError {
  const firstIssue = error.issues[0];
  const path = firstIssue?.path.join(".");

  return {
    code: "invalid-wire-payload",
    message: firstIssue?.message ?? "Payload did not match the expected shape.",
    source: "parse",
    ...(path ? { path } : {}),
    details: error.issues,
  };
}
