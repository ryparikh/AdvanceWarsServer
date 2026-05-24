import type { Action, Coordinate } from "../gameState/schema";
import { coordinateKey, sameCoordinate } from "../gameState/schema";

type HighlightTile = {
  x: number;
  y: number;
  key: string;
};

export type ActionHighlights = {
  moves: HighlightTile[];
  attacks: HighlightTile[];
  actionsByTile: Map<string, Action[]>;
};

const moveActionTypes = new Set([
  "move-wait",
  "move-capture",
  "move-load",
  "move-combine",
  "move-attack"
]);

function directionalTarget(origin: Coordinate, direction: Action["direction"]): Coordinate | undefined {
  if (direction === "north") {
    return [origin[0], origin[1] - 1];
  }
  if (direction === "east") {
    return [origin[0] + 1, origin[1]];
  }
  if (direction === "south") {
    return [origin[0], origin[1] + 1];
  }
  if (direction === "west") {
    return [origin[0] - 1, origin[1]];
  }
  return undefined;
}

function pushHighlight(
  target: Coordinate,
  collection: HighlightTile[],
  actionsByTile: Map<string, Action[]>,
  action: Action
) {
  const key = coordinateKey(target);
  collection.push({ x: target[0], y: target[1], key });
  actionsByTile.set(key, [...(actionsByTile.get(key) ?? []), action]);
}

export function actionHighlightsForSource(actions: Action[], source: Coordinate): ActionHighlights {
  const moves: HighlightTile[] = [];
  const attacks: HighlightTile[] = [];
  const actionsByTile = new Map<string, Action[]>();

  for (const action of actions) {
    if (!sameCoordinate(action.source, source)) {
      continue;
    }

    if (action.target && moveActionTypes.has(action.type)) {
      pushHighlight(action.target, moves, actionsByTile, action);
    }

    if (action.type === "attack" && action.target) {
      pushHighlight(action.target, attacks, actionsByTile, action);
    }

    if (action.type === "move-attack" && action.target && action.direction) {
      const target = directionalTarget(action.target, action.direction);
      if (target) {
        pushHighlight(target, attacks, actionsByTile, action);
      }
    }
  }

  return { moves, attacks, actionsByTile };
}
