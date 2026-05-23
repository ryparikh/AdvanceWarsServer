import {
  GameStateWireSchema,
  type GameStateWire,
  type PlayerWire,
  type UnitWire,
} from "../contract/wire";
import type {
  Board,
  BoardTile,
  GameState,
  LoadedUnit,
  LuckPolicy,
  OwnerRef,
  Player,
  PlayerId,
  PropertyInfo,
  Unit,
} from "../domain/types";
import { fail, ok, type ApiResult, zodParseError } from "./result";

function normalizeLuckPolicy(policy: 0 | 1 | 2 | 3): LuckPolicy {
  switch (policy) {
    case 0:
      return "normal";
    case 1:
      return "lowest";
    case 2:
      return "highest";
    case 3:
      return "middle";
  }
}

function normalizePlayer(player: PlayerWire, id: PlayerId): Player {
  return {
    id,
    armyType: player.armyType,
    co: player.co,
    funds: player.funds,
    luckPolicy: normalizeLuckPolicy(player["luck-policy"]),
    powerMeter: {
      copStars: player["power-meter"]["cop-stars"],
      scopStars: player["power-meter"]["scop-stars"],
      charge: player["power-meter"].charge,
      starValue: player["power-meter"]["star-value"],
    },
    powerStatus: player["power-status"],
  };
}

function playerIdForArmy(
  armyType: string,
  players: readonly Player[],
): PlayerId | undefined {
  return players.find((player) => player.armyType === armyType)?.id;
}

function normalizeOwner(owner: string, players: readonly Player[]): OwnerRef | null {
  if (owner === "neutral") {
    return { kind: "neutral" };
  }

  const playerId = playerIdForArmy(owner, players);
  return playerId === undefined ? null : { kind: "player", playerId };
}

function normalizeLoadedUnit(
  unit: UnitWire,
  players: readonly Player[],
): ApiResult<LoadedUnit> {
  const ownerPlayerId = playerIdForArmy(unit.owner, players);
  if (ownerPlayerId === undefined) {
    return fail({
      code: "unknown-unit-owner",
      message: `Unit owner '${unit.owner}' does not match any player army.`,
      source: "validation",
      path: "unit.owner",
    });
  }

  const loadedUnits: LoadedUnit[] = [];
  for (const loadedUnit of unit["loaded-units"] ?? []) {
    const normalized = normalizeLoadedUnit(loadedUnit, players);
    if (!normalized.ok) {
      return normalized;
    }
    loadedUnits.push(normalized.data);
  }

  const normalizedUnit: LoadedUnit = {
    type: unit.type,
    ownerPlayerId,
    displayHealth: unit["display-health"],
    ammo: unit.ammo,
    fuel: unit.fuel,
    moved: unit.moved,
    hidden: unit.hidden,
    ...(unit.stunned !== undefined ? { stunned: unit.stunned } : {}),
    ...(loadedUnits.length > 0 ? { loadedUnits } : {}),
  };

  if (unit.health !== undefined) {
    normalizedUnit.health = unit.health;
  }

  return ok(normalizedUnit);
}

function normalizeUnit(
  unit: UnitWire,
  players: readonly Player[],
): ApiResult<Unit> {
  return normalizeLoadedUnit(unit, players);
}

function normalizeBoard(
  gameState: GameStateWire,
  players: readonly Player[],
): ApiResult<Board> {
  const firstRow = gameState.map[0];
  if (!firstRow || firstRow.length === 0) {
    return fail({
      code: "invalid-map-shape",
      message: "Game map must contain at least one row and one column.",
      source: "validation",
      path: "map",
    });
  }

  const width = firstRow.length;
  const tiles: BoardTile[][] = [];

  for (const [y, row] of gameState.map.entries()) {
    if (row.length !== width) {
      return fail({
        code: "invalid-map-shape",
        message: "Game map rows must all have the same width.",
        source: "validation",
        path: `map.${y}`,
      });
    }

    const normalizedRow: BoardTile[] = [];
    for (const [x, tile] of row.entries()) {
      const normalizedTile: BoardTile = {
        x,
        y,
        terrainId: tile.terrain,
      };

      if (tile.property) {
        const owner = normalizeOwner(tile.property.owner, players);
        if (!owner) {
          return fail({
            code: "unknown-property-owner",
            message: `Property owner '${tile.property.owner}' does not match any player army or neutral.`,
            source: "validation",
            path: `map.${y}.${x}.property.owner`,
          });
        }

        const property: PropertyInfo = {
          capturePoints: tile.property["capture-points"],
          owner,
        };
        normalizedTile.property = property;
      }

      if (tile.unit) {
        const normalizedUnit = normalizeUnit(tile.unit, players);
        if (!normalizedUnit.ok) {
          return normalizedUnit;
        }
        normalizedTile.unit = normalizedUnit.data;
      }

      normalizedRow.push(normalizedTile);
    }

    tiles.push(normalizedRow);
  }

  return ok({
    width,
    height: gameState.map.length,
    tiles,
  });
}

function validatePlayerId(
  playerId: number,
  players: readonly Player[],
  path: string,
): ApiResult<PlayerId> {
  if (!players[playerId]) {
    return fail({
      code: "invalid-player-id",
      message: `Player id '${playerId}' does not match an entry in players.`,
      source: "validation",
      path,
    });
  }

  return ok(playerId);
}

function validateUniquePlayerArmies(players: readonly Player[]): ApiResult<void> {
  const seen = new Map<string, PlayerId>();

  for (const player of players) {
    const existingPlayerId = seen.get(player.armyType);
    if (existingPlayerId !== undefined) {
      return fail({
        code: "duplicate-player-army",
        message: `Players '${existingPlayerId}' and '${player.id}' both use army '${player.armyType}'.`,
        source: "validation",
        path: `players.${player.id}.armyType`,
      });
    }

    seen.set(player.armyType, player.id);
  }

  return ok(undefined);
}

function normalizeGameState(gameState: GameStateWire): ApiResult<GameState> {
  const players = gameState.players.map((player, id) =>
    normalizePlayer(player, id),
  );

  const uniquePlayerArmies = validateUniquePlayerArmies(players);
  if (!uniquePlayerArmies.ok) {
    return uniquePlayerArmies;
  }

  const activePlayerId = validatePlayerId(
    gameState.activePlayer,
    players,
    "activePlayer",
  );
  if (!activePlayerId.ok) {
    return activePlayerId;
  }

  const board = normalizeBoard(gameState, players);
  if (!board.ok) {
    return board;
  }

  let winnerPlayerId: PlayerId | null;
  if (gameState.winner === -1) {
    winnerPlayerId = null;
  } else if (gameState.winner >= 0) {
    const winner = validatePlayerId(gameState.winner, players, "winner");
    if (!winner.ok) {
      return winner;
    }
    winnerPlayerId = winner.data;
  } else {
    return fail({
      code: "invalid-winner",
      message: "Winner must be -1 or a valid player id.",
      source: "validation",
      path: "winner",
    });
  }

  return ok({
    gameId: gameState.gameId,
    activePlayerId: activePlayerId.data,
    turnCount: gameState["turn-count"],
    gameOver: gameState["game-over"],
    winnerPlayerId,
    weather: gameState.weather ?? "clear",
    settings: {
      unitCap: gameState["unit-cap"],
      captureLimit: gameState["cap-limit"],
    },
    players,
    board: board.data,
  });
}

export function parseGameState(input: unknown): ApiResult<GameState> {
  const parsed = GameStateWireSchema.safeParse(input);
  if (!parsed.success) {
    return fail(zodParseError(parsed.error));
  }

  return normalizeGameState(parsed.data);
}
