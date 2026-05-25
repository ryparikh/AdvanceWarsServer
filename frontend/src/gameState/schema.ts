import { z } from "zod";

const maxMapRows = 64;
const maxMapColumns = 64;
const maxMapTiles = maxMapRows * maxMapColumns;
const maxTextLength = 80;
const maxActionCount = 512;
const maxLoadedUnits = 2;

const finiteInteger = z.number().finite().int();
const shortText = z.string().min(1).max(maxTextLength);
const coordinateValueSchema = finiteInteger.min(0).max(Math.max(maxMapRows, maxMapColumns) - 1);
const coordinateSchema = z.tuple([coordinateValueSchema, coordinateValueSchema]);

export const actionSchema = z.object({
  type: shortText,
  source: coordinateSchema.optional(),
  target: coordinateSchema.optional(),
  direction: z.enum(["north", "east", "south", "west"]).optional(),
  unit: shortText.optional(),
  unloadIndex: finiteInteger.min(0).max(maxLoadedUnits - 1).optional()
});

export type Action = z.infer<typeof actionSchema>;
export type Coordinate = z.infer<typeof coordinateSchema>;
export const actionListSchema = z.array(actionSchema).max(maxActionCount);
export const legalActionEnvelopeSchema = z.object({
  gameId: shortText,
  activePlayer: finiteInteger.min(0).max(7),
  source: coordinateSchema.optional(),
  actions: actionListSchema
});

const powerMeterSchema = z.object({
  "cop-stars": finiteInteger.min(0).max(20),
  "scop-stars": finiteInteger.min(0).max(20),
  charge: finiteInteger.min(0).max(1_000_000),
  "star-value": finiteInteger.min(1).max(1_000_000),
  "cop-threshold": finiteInteger.min(0).max(1_000_000).optional(),
  "scop-threshold": finiteInteger.min(0).max(1_000_000).optional(),
  "can-use-cop": z.boolean().optional(),
  "can-use-scop": z.boolean().optional()
});

export const playerSchema = z.object({
  armyType: shortText,
  co: shortText,
  funds: finiteInteger.min(0).max(99_999_999),
  "power-meter": powerMeterSchema,
  "power-status": finiteInteger.min(0).max(3),
  "luck-policy": finiteInteger.min(0).max(10).optional()
});

export type Player = z.infer<typeof playerSchema>;

const propertySchema = z.object({
  "capture-points": finiteInteger.min(0).max(20),
  owner: shortText
});

const unitCoreFields = {
  type: shortText,
  ammo: finiteInteger.min(0).max(99),
  fuel: finiteInteger.min(0).max(99),
  health: finiteInteger.min(0).max(100),
  owner: shortText,
  moved: z.boolean(),
  hidden: z.boolean(),
  stunned: z.boolean().optional()
};

const loadedUnitSchema = z.object({
  ...unitCoreFields,
  "loaded-units": z.undefined({ invalid_type_error: "loaded units cannot contain nested cargo" }).optional()
});

export const unitSchema = z.object({
  ...unitCoreFields,
  "loaded-units": z.array(loadedUnitSchema).max(maxLoadedUnits).optional()
});

export type Unit = z.infer<typeof unitSchema>;

export const mapTileSchema = z.object({
  terrain: finiteInteger.min(0).max(1_000),
  property: propertySchema.optional(),
  unit: unitSchema.optional()
});

export type MapTile = z.infer<typeof mapTileSchema>;

const mapSchema = z.array(z.array(mapTileSchema).min(1).max(maxMapColumns)).min(1).max(maxMapRows).superRefine((map, ctx) => {
  const expectedWidth = map[0]?.length ?? 0;
  const tileCount = map.reduce((total, row) => total + row.length, 0);

  if (tileCount > maxMapTiles) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      message: `map cannot exceed ${maxMapTiles} tiles`
    });
  }

  map.forEach((row, rowIndex) => {
    if (row.length !== expectedWidth) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: [rowIndex],
        message: "map rows must be rectangular"
      });
    }
  });
});

export const gameStateSchema = z.object({
  gameId: shortText,
  map: mapSchema,
  players: z.array(playerSchema).min(1).max(8),
  "unit-cap": finiteInteger.min(0).max(500),
  "cap-limit": finiteInteger.min(0).max(500),
  "turn-count": finiteInteger.min(0).max(10_000),
  activePlayer: finiteInteger.min(0).max(7),
  "game-over": z.boolean(),
  winner: finiteInteger.min(-1).max(7),
  terminalReason: shortText.nullable().optional(),
  settings: z.record(z.unknown()).optional(),
  weather: z.enum(["clear", "rain", "snow"]).optional(),
  "weather-turns-remaining": finiteInteger.min(0).max(100).optional(),
  "combat-rng-seed": finiteInteger.min(0).max(4_294_967_295).optional()
}).superRefine((gameState, ctx) => {
  if (gameState.activePlayer >= gameState.players.length) {
    ctx.addIssue({
      code: z.ZodIssueCode.custom,
      path: ["activePlayer"],
      message: "activePlayer must reference an existing player"
    });
  }
});

export type GameState = z.infer<typeof gameStateSchema>;

export const validActionGroupSchema = z.object({
  source: coordinateSchema,
  expected: actionListSchema
});

export type ValidActionGroup = z.infer<typeof validActionGroupSchema>;

const fixturePayloadSchema = z.object({
  "initial-game-state": gameStateSchema,
  validActions: z.array(validActionGroupSchema).max(maxActionCount).optional()
});

export type ParsedGameStatePayload = {
  gameState: GameState;
  legalActionGroups: ValidActionGroup[];
};

function hasFixtureShape(payload: unknown): payload is { "initial-game-state": unknown } {
  return typeof payload === "object" && payload !== null && "initial-game-state" in payload;
}

export function parseGameStatePayload(payload: unknown): ParsedGameStatePayload {
  if (hasFixtureShape(payload)) {
    const fixture = fixturePayloadSchema.parse(payload);
    return {
      gameState: fixture["initial-game-state"],
      legalActionGroups: fixture.validActions ?? []
    };
  }

  return {
    gameState: gameStateSchema.parse(payload),
    legalActionGroups: []
  };
}

export function sameCoordinate(a: Coordinate | undefined, b: Coordinate): boolean {
  return a !== undefined && a[0] === b[0] && a[1] === b[1];
}

export function coordinateKey(coordinate: Coordinate): string {
  return `${coordinate[0]},${coordinate[1]}`;
}
