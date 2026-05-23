import { z } from "zod";

import {
  ActionTypeSchema,
  ArmyTypeSchema,
  CommandingOfficerSchema,
  DirectionSchema,
  OwnerSchema,
  UnitTypeSchema,
  type ArmyType,
  type UnitType,
} from "./ids";

export const CoordinateWireSchema = z.tuple([
  z.number().int().nonnegative(),
  z.number().int().nonnegative(),
]);
export type CoordinateWire = z.infer<typeof CoordinateWireSchema>;

export const LuckPolicyWireSchema = z.union([
  z.literal(0),
  z.literal(1),
  z.literal(2),
  z.literal(3),
]);
export type LuckPolicyWire = z.infer<typeof LuckPolicyWireSchema>;

export const PowerMeterWireSchema = z
  .object({
    "cop-stars": z.number().int().nonnegative(),
    "scop-stars": z.number().int().nonnegative(),
    charge: z.number().int().nonnegative(),
    "star-value": z.number().int().positive(),
  })
  .passthrough();
export type PowerMeterWire = z.infer<typeof PowerMeterWireSchema>;

export const PlayerWireSchema = z
  .object({
    armyType: ArmyTypeSchema,
    co: CommandingOfficerSchema,
    funds: z.number().int().nonnegative(),
    "luck-policy": LuckPolicyWireSchema,
    "power-meter": PowerMeterWireSchema,
    "power-status": z.number().int().min(0).max(2),
  })
  .passthrough();
export type PlayerWire = z.infer<typeof PlayerWireSchema>;

export type UnitWire = {
  type: UnitType;
  owner: ArmyType;
  health?: number;
  "display-health": number;
  ammo: number;
  fuel: number;
  moved: boolean;
  hidden: boolean;
  stunned?: boolean;
  "loaded-units"?: UnitWire[];
};

export const UnitWireSchema: z.ZodType<UnitWire> = z.lazy(() =>
  z
    .object({
      type: UnitTypeSchema,
      owner: ArmyTypeSchema,
      health: z.number().int().min(0).max(100).optional(),
      "display-health": z.number().int().min(0).max(10),
      ammo: z.number().int().nonnegative(),
      fuel: z.number().int().nonnegative(),
      moved: z.boolean(),
      hidden: z.boolean(),
      stunned: z.boolean().optional(),
      "loaded-units": z.array(UnitWireSchema).optional(),
    })
    .passthrough(),
) as z.ZodType<UnitWire>;

export const PropertyWireSchema = z
  .object({
    "capture-points": z.number().int().nonnegative(),
    owner: OwnerSchema,
  })
  .passthrough();
export type PropertyWire = z.infer<typeof PropertyWireSchema>;

export const MapTileWireSchema = z
  .object({
    terrain: z.number().int().nonnegative(),
    property: PropertyWireSchema.optional(),
    unit: UnitWireSchema.optional(),
  })
  .passthrough();
export type MapTileWire = z.infer<typeof MapTileWireSchema>;

export const WeatherWireSchema = z.enum(["clear", "rain", "snow"]);
export type WeatherWire = z.infer<typeof WeatherWireSchema>;

export const GameStateWireSchema = z
  .object({
    gameId: z.string().min(1),
    map: z.array(z.array(MapTileWireSchema)),
    players: z.tuple([PlayerWireSchema, PlayerWireSchema]),
    "unit-cap": z.number().int().nonnegative(),
    "cap-limit": z.number().int().nonnegative(),
    "turn-count": z.number().int().nonnegative(),
    activePlayer: z.number().int().nonnegative(),
    "game-over": z.boolean(),
    winner: z.number().int(),
    weather: WeatherWireSchema.optional(),
    "weather-turns-remaining": z.number().int().nonnegative().optional(),
    "combat-rng-seed": z.number().int().nonnegative().optional(),
  })
  .passthrough();
export type GameStateWire = z.infer<typeof GameStateWireSchema>;

export const ActionWireSchema = z
  .object({
    type: ActionTypeSchema,
    source: CoordinateWireSchema.optional(),
    target: CoordinateWireSchema.optional(),
    direction: DirectionSchema.optional(),
    unit: UnitTypeSchema.optional(),
    unloadIndex: z.number().int().nonnegative().optional(),
  })
  .passthrough();
export type ActionWire = z.infer<typeof ActionWireSchema>;
