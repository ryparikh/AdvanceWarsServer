import type {
  ArmyType,
  CommandingOfficer,
  Direction,
  UnitType,
} from "../contract/ids";

export type PlayerId = number;

export type Coordinate = {
  x: number;
  y: number;
};

export type Weather = "clear" | "rain" | "snow";

export type LuckPolicy = "normal" | "lowest" | "highest" | "middle";

export type OwnerRef =
  | { kind: "neutral" }
  | { kind: "player"; playerId: PlayerId };

export type PowerMeter = {
  copStars: number;
  scopStars: number;
  charge: number;
  starValue: number;
};

export type Player = {
  id: PlayerId;
  armyType: ArmyType;
  co: CommandingOfficer;
  funds: number;
  luckPolicy: LuckPolicy;
  powerMeter: PowerMeter;
  powerStatus: number;
};

export type GameSettings = {
  unitCap: number;
  captureLimit: number;
};

export type LoadedUnit = {
  type: UnitType;
  ownerPlayerId: PlayerId;
  health?: number;
  displayHealth: number;
  ammo: number;
  fuel: number;
  moved: boolean;
  hidden: boolean;
  stunned?: boolean;
  loadedUnits?: LoadedUnit[];
};

export type Unit = LoadedUnit;

export type PropertyInfo = {
  capturePoints: number;
  owner: OwnerRef;
};

export type BoardTile = {
  x: number;
  y: number;
  terrainId: number;
  property?: PropertyInfo;
  unit?: Unit;
};

export type Board = {
  width: number;
  height: number;
  tiles: BoardTile[][];
};

export type GameState = {
  gameId: string;
  activePlayerId: PlayerId;
  turnCount: number;
  gameOver: boolean;
  winnerPlayerId: PlayerId | null;
  weather: Weather;
  settings: GameSettings;
  players: Player[];
  board: Board;
};

export type EndTurnAction = {
  type: "end-turn";
};

export type PowerAction = {
  type: "co-power" | "super-co-power";
};

export type TargetAction = {
  type:
    | "attack"
    | "move-capture"
    | "move-combine"
    | "move-load"
    | "move-wait";
  source: Coordinate;
  target: Coordinate;
};

export type MoveAttackAction = {
  type: "move-attack";
  source: Coordinate;
  target: Coordinate;
  direction: Direction;
};

export type DirectionalAction = {
  type: "repair";
  source: Coordinate;
  direction: Direction;
};

export type UnloadAction = {
  type: "unload";
  source: Coordinate;
  direction: Direction;
  unloadIndex: number;
};

export type BuyAction = {
  type: "buy";
  source: Coordinate;
  unitType: UnitType;
};

export type Action =
  | EndTurnAction
  | PowerAction
  | TargetAction
  | MoveAttackAction
  | DirectionalAction
  | UnloadAction
  | BuyAction;
