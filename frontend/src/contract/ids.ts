import { z } from "zod";

export const ArmyTypeSchema = z.enum(["orange-star", "blue-moon"]);
export type ArmyType = z.infer<typeof ArmyTypeSchema>;

export const OwnerSchema = z.union([ArmyTypeSchema, z.literal("neutral")]);
export type Owner = z.infer<typeof OwnerSchema>;

export const CommandingOfficerSchema = z.enum([
  "Adder",
  "Andy",
  "Colin",
  "Drake",
  "Eagle",
  "Flak",
  "Grimm",
  "Grit",
  "Hachi",
  "Hawke",
  "Jake",
  "Javier",
  "Jess",
  "Jugger",
  "Kanbei",
  "Kindle",
  "Koal",
  "Lash",
  "Max",
  "Nell",
  "Olaf",
  "Rachel",
  "Sami",
  "Sasha",
  "Sensei",
  "Sonja",
  "Sturm",
  "VonBolt",
]);
export type CommandingOfficer = z.infer<typeof CommandingOfficerSchema>;

export const UnitTypeSchema = z.enum([
  "anti-air",
  "apc",
  "artillery",
  "bcopter",
  "battleship",
  "blackboat",
  "blackbomb",
  "bomber",
  "carrier",
  "cruiser",
  "fighter",
  "infantry",
  "lander",
  "medium-tank",
  "mech",
  "megatank",
  "missile",
  "neotank",
  "piperunner",
  "recon",
  "rocket",
  "stealth",
  "sub",
  "tcopter",
  "tank",
]);
export type UnitType = z.infer<typeof UnitTypeSchema>;

export const DirectionSchema = z.enum(["north", "east", "south", "west"]);
export type Direction = z.infer<typeof DirectionSchema>;

export const ActionTypeSchema = z.enum([
  "attack",
  "buy",
  "co-power",
  "end-turn",
  "move-attack",
  "move-capture",
  "move-combine",
  "move-load",
  "move-wait",
  "repair",
  "super-co-power",
  "unload",
]);
export type ActionType = z.infer<typeof ActionTypeSchema>;
