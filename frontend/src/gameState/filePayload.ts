import { parseGameStatePayload, type ParsedGameStatePayload } from "./schema";
import { isVisualizerTracePayload, parseVisualizerTracePayload, type ParsedVisualizerTrace } from "./trace";

export type ParsedLoadedPayload = ParsedGameStatePayload & {
  trace?: ParsedVisualizerTrace;
};

function selectedFileLabel(fileName: string): string {
  return fileName.trim() || "selected file";
}

function parseJson(text: string, fileName: string): unknown {
  try {
    return JSON.parse(text);
  } catch (err) {
    const detail = err instanceof Error ? err.message : "invalid JSON";
    throw new Error(`Unable to parse ${selectedFileLabel(fileName)}: ${detail}`);
  }
}

function parseJsonLine(line: string, lineNumber: number, fileName: string): unknown {
  try {
    return JSON.parse(line);
  } catch (err) {
    const detail = err instanceof Error ? err.message : "invalid JSON";
    throw new Error(`Unable to parse ${selectedFileLabel(fileName)} line ${lineNumber}: ${detail}`);
  }
}

function isReplayGameRecord(payload: unknown): payload is { initialState: unknown } {
  return (
    typeof payload === "object" &&
    payload !== null &&
    "recordType" in payload &&
    payload.recordType === "game" &&
    "initialState" in payload
  );
}

function parseReplayJsonl(text: string, fileName: string): ParsedLoadedPayload {
  const lines = text.split(/\r?\n/);

  for (let index = 0; index < lines.length; index += 1) {
    const line = lines[index].trim();
    if (line.length === 0) {
      continue;
    }

    const payload = parseJsonLine(line, index + 1, fileName);
    if (isReplayGameRecord(payload)) {
      return parseGameStatePayload(payload.initialState);
    }
  }

  throw new Error(`${selectedFileLabel(fileName)} does not contain a replay game record`);
}

export function parseLoadedPayload(payload: unknown): ParsedLoadedPayload {
  if (isVisualizerTracePayload(payload)) {
    const trace = parseVisualizerTracePayload(payload);
    return {
      gameState: trace.initialState,
      legalActionGroups: [],
      trace
    };
  }

  return parseGameStatePayload(payload);
}

export function parseGameStateFileText(text: string, fileName = "selected file"): ParsedLoadedPayload {
  if (text.trim().length === 0) {
    throw new Error(`${selectedFileLabel(fileName)} is empty`);
  }

  if (fileName.toLowerCase().endsWith(".jsonl")) {
    return parseReplayJsonl(text, fileName);
  }

  const payload = parseJson(text, fileName);
  if (isReplayGameRecord(payload)) {
    return parseGameStatePayload(payload.initialState);
  }

  return parseLoadedPayload(payload);
}
