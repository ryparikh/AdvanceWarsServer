import { z } from "zod";
import { actionSchema, gameStateSchema, type Coordinate, type GameState } from "./schema";

const maxTraceSteps = 5_000;
const maxActionIndex = 10_000_000;
const maxVisitCountEntries = 100_000;

const traceMetricSchema = z.record(z.unknown());

const visitCountSchema = z.object({
  actionIndex: z.number().finite().int().min(0).max(maxActionIndex),
  visits: z.number().finite().int().min(1).max(1_000_000)
});

const traceStepSchema = z.object({
  ply: z.number().finite().int().min(0).max(maxTraceSteps),
  player: z.number().finite().int().min(0).max(7),
  actionIndex: z.number().finite().int().min(0).max(maxActionIndex),
  action: actionSchema,
  legalActionCount: z.number().finite().int().min(0).max(100_000),
  selectedActionIndex: z.number().finite().int().min(0).max(maxActionIndex),
  stateTensorChecksum: z.string().min(1).max(64).optional(),
  visitCounts: z.array(visitCountSchema).max(maxVisitCountEntries).optional(),
  mcts: traceMetricSchema.optional(),
  resultingState: gameStateSchema
});

export type VisualizerTraceStep = z.infer<typeof traceStepSchema>;

export const visualizerTracePayloadSchema = z.object({
  traceFormatVersion: z.literal("standard-gl-visualizer-trace-v1"),
  source: traceMetricSchema.optional(),
  initialState: gameStateSchema,
  steps: z.array(traceStepSchema).max(maxTraceSteps),
  finalState: gameStateSchema.optional(),
  terminalReason: z.string().min(1).max(80).nullable().optional(),
  winner: z.number().finite().int().min(-1).max(7).nullable().optional(),
  metrics: traceMetricSchema.optional()
}).superRefine((trace, ctx) => {
  trace.steps.forEach((step, index) => {
    if (step.ply !== index) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ["steps", index, "ply"],
        message: "step ply must match its timeline index"
      });
    }
  });
});

type VisualizerTracePayload = z.infer<typeof visualizerTracePayloadSchema>;

export type ParsedVisualizerTrace = Omit<VisualizerTracePayload, "traceFormatVersion"> & {
  traceFormatVersion: "standard-gl-visualizer-trace-v1";
  states: GameState[];
};

export function isVisualizerTracePayload(payload: unknown): payload is { traceFormatVersion: unknown } {
  return (
    typeof payload === "object" &&
    payload !== null &&
    "traceFormatVersion" in payload
  );
}

export function parseVisualizerTracePayload(payload: unknown): ParsedVisualizerTrace {
  const trace = visualizerTracePayloadSchema.parse(payload);
  return {
    ...trace,
    states: [trace.initialState, ...trace.steps.map((step) => step.resultingState)]
  };
}

export function stateForTraceIndex(trace: ParsedVisualizerTrace, index: number): GameState {
  const clampedIndex = Math.max(0, Math.min(index, trace.states.length - 1));
  return trace.states[clampedIndex];
}

export function stepForTraceIndex(trace: ParsedVisualizerTrace, index: number): VisualizerTraceStep | undefined {
  if (index <= 0) {
    return undefined;
  }

  return trace.steps[index - 1];
}

export function changedTilesBetween(before: GameState, after: GameState): Coordinate[] {
  const changes: Coordinate[] = [];
  const rows = Math.max(before.map.length, after.map.length);
  for (let y = 0; y < rows; y += 1) {
    const cols = Math.max(before.map[y]?.length ?? 0, after.map[y]?.length ?? 0);
    for (let x = 0; x < cols; x += 1) {
      if (JSON.stringify(before.map[y]?.[x] ?? null) !== JSON.stringify(after.map[y]?.[x] ?? null)) {
        changes.push([x, y]);
      }
    }
  }

  return changes;
}
