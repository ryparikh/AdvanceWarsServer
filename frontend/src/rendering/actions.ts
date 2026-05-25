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

export type ActionPreview =
  | {
      kind: "move";
      path: Coordinate[];
    }
  | {
      kind: "attack";
      attacker: Coordinate;
      attackerUnit: Coordinate;
      defender: Coordinate;
      route?: Coordinate[];
    };

const moveActionTypes = new Set([
  "move-wait",
  "move-hide",
  "move-unhide",
  "move-capture",
  "move-load",
  "move-combine",
  "move-attack"
]);

const movementPreviewTypes = new Set([
  "move-wait",
  "move-hide",
  "move-unhide",
  "move-capture",
  "move-load",
  "move-combine"
]);

const movementDirections: Coordinate[] = [
  [0, -1],
  [1, 0],
  [0, 1],
  [-1, 0]
];

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

export function orthogonalPath(source: Coordinate, target: Coordinate): Coordinate[] {
  if (sameCoordinate(source, target)) {
    return [source];
  }

  const path: Coordinate[] = [source];
  const bend: Coordinate = [source[0], target[1]];
  if (!sameCoordinate(source, bend) && !sameCoordinate(bend, target)) {
    path.push(bend);
  }
  path.push(target);
  return path;
}

function isAdjacent(a: Coordinate, b: Coordinate): boolean {
  return Math.abs(a[0] - b[0]) + Math.abs(a[1] - b[1]) === 1;
}

function routeForTarget(
  source: Coordinate,
  target: Coordinate,
  movementTrail?: Coordinate[]
): Coordinate[] {
  if (movementTrail?.length && sameCoordinate(movementTrail[0], source)) {
    const targetIndex = movementTrail.findIndex((coordinate) => sameCoordinate(coordinate, target));
    if (targetIndex >= 0) {
      return movementTrail.slice(0, targetIndex + 1);
    }
  }

  return orthogonalPath(source, target);
}

function moveTargetKeys(highlights: ActionHighlights, source: Coordinate): Set<string> {
  return new Set([
    coordinateKey(source),
    ...highlights.moves.map((move) => move.key)
  ]);
}

function shortestPathWithin(validKeys: Set<string>, source: Coordinate, target: Coordinate): Coordinate[] | undefined {
  const sourceKey = coordinateKey(source);
  const targetKey = coordinateKey(target);
  const queue: Coordinate[] = [source];
  const previous = new Map<string, string | undefined>([[sourceKey, undefined]]);

  while (queue.length > 0) {
    const current = queue.shift();
    if (!current) {
      break;
    }

    if (sameCoordinate(current, target)) {
      const path: Coordinate[] = [];
      let key: string | undefined = targetKey;
      while (key) {
        const [x, y] = key.split(",").map(Number);
        path.push([x, y]);
        key = previous.get(key);
      }
      return path.reverse();
    }

    for (const [dx, dy] of movementDirections) {
      const neighbor: Coordinate = [current[0] + dx, current[1] + dy];
      const neighborKey = coordinateKey(neighbor);
      if (!validKeys.has(neighborKey) || previous.has(neighborKey)) {
        continue;
      }

      previous.set(neighborKey, coordinateKey(current));
      queue.push(neighbor);
    }
  }

  return undefined;
}

export function movementTrailAfterHover(
  currentTrail: Coordinate[] | undefined,
  source: Coordinate | undefined,
  hover: Coordinate | undefined,
  highlights: ActionHighlights | undefined
): Coordinate[] | undefined {
  if (!source || !highlights) {
    return undefined;
  }

  const validKeys = moveTargetKeys(highlights, source);
  const trail = currentTrail?.length && sameCoordinate(currentTrail[0], source) ? currentTrail : [source];
  if (!hover || !validKeys.has(coordinateKey(hover))) {
    return trail;
  }

  const existingIndex = trail.findIndex((coordinate) => sameCoordinate(coordinate, hover));
  if (existingIndex >= 0) {
    return trail.slice(0, existingIndex + 1);
  }

  const last = trail[trail.length - 1];
  if (isAdjacent(last, hover)) {
    return [...trail, hover];
  }

  const trailingPath = shortestPathWithin(validKeys, last, hover);
  if (trailingPath && trailingPath.length > 1) {
    return [...trail, ...trailingPath.slice(1)];
  }

  return shortestPathWithin(validKeys, source, hover) ?? trail;
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

export function actionPreviewForTile(
  actions: Action[],
  source: Coordinate,
  tile: Coordinate,
  movementTrail?: Coordinate[]
): ActionPreview | undefined {
  const attack = actions.find((action) => {
    if (!sameCoordinate(action.source, source)) {
      return false;
    }

    if (action.type === "attack") {
      return sameCoordinate(action.target, tile);
    }

    if (action.type === "move-attack" && action.target && action.direction) {
      const defender = directionalTarget(action.target, action.direction);
      return sameCoordinate(defender, tile);
    }

    return false;
  });

  if (attack?.type === "attack" && attack.target) {
    return {
      kind: "attack",
      attacker: source,
      attackerUnit: source,
      defender: attack.target,
      route: undefined
    };
  }

  if (attack?.type === "move-attack" && attack.target && attack.direction) {
    const defender = directionalTarget(attack.target, attack.direction);
    if (defender) {
      return {
        kind: "attack",
        attacker: attack.target,
        attackerUnit: source,
        defender,
        route: routeForTarget(source, attack.target, movementTrail)
      };
    }
  }

  const move = actions.find((action) =>
    sameCoordinate(action.source, source) &&
    action.target !== undefined &&
    sameCoordinate(action.target, tile) &&
    movementPreviewTypes.has(action.type)
  );

  if (move?.target) {
    return {
      kind: "move",
      path: routeForTarget(source, move.target, movementTrail)
    };
  }

  return undefined;
}
