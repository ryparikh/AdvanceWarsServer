import { z } from "zod";

import {
  ActionWireSchema,
  type ActionWire,
  type CoordinateWire,
} from "../contract/wire";
import type { Action, Coordinate } from "../domain/types";
import { fail, ok, type ApiResult, zodParseError } from "./result";

const ActionsWireSchema = z.array(ActionWireSchema);

function normalizeCoordinate([x, y]: CoordinateWire): Coordinate {
  return { x, y };
}

function serializeCoordinate({ x, y }: Coordinate): CoordinateWire {
  return [x, y];
}

function missingField(type: string, field: string): ApiResult<Action> {
  return fail({
    code: "invalid-action-shape",
    message: `Action '${type}' is missing required field '${field}'.`,
    source: "validation",
    path: field,
  });
}

export function normalizeAction(action: ActionWire): ApiResult<Action> {
  switch (action.type) {
    case "end-turn":
    case "co-power":
    case "super-co-power":
      return ok({ type: action.type });

    case "attack":
    case "move-capture":
    case "move-combine":
    case "move-load":
    case "move-wait":
      if (!action.source) {
        return missingField(action.type, "source");
      }
      if (!action.target) {
        return missingField(action.type, "target");
      }
      return ok({
        type: action.type,
        source: normalizeCoordinate(action.source),
        target: normalizeCoordinate(action.target),
      });

    case "move-attack":
      if (!action.source) {
        return missingField(action.type, "source");
      }
      if (!action.target) {
        return missingField(action.type, "target");
      }
      if (!action.direction) {
        return missingField(action.type, "direction");
      }
      return ok({
        type: action.type,
        source: normalizeCoordinate(action.source),
        target: normalizeCoordinate(action.target),
        direction: action.direction,
      });

    case "repair":
      if (!action.source) {
        return missingField(action.type, "source");
      }
      if (!action.direction) {
        return missingField(action.type, "direction");
      }
      return ok({
        type: action.type,
        source: normalizeCoordinate(action.source),
        direction: action.direction,
      });

    case "unload":
      if (!action.source) {
        return missingField(action.type, "source");
      }
      if (!action.direction) {
        return missingField(action.type, "direction");
      }
      if (action.unloadIndex === undefined) {
        return missingField(action.type, "unloadIndex");
      }
      return ok({
        type: action.type,
        source: normalizeCoordinate(action.source),
        direction: action.direction,
        unloadIndex: action.unloadIndex,
      });

    case "buy":
      if (!action.source) {
        return missingField(action.type, "source");
      }
      if (!action.unit) {
        return missingField(action.type, "unit");
      }
      return ok({
        type: action.type,
        source: normalizeCoordinate(action.source),
        unitType: action.unit,
      });
  }
}

export function parseActions(input: unknown): ApiResult<Action[]> {
  const parsed = ActionsWireSchema.safeParse(input);
  if (!parsed.success) {
    return fail(zodParseError(parsed.error));
  }

  const actions: Action[] = [];
  for (const action of parsed.data) {
    const normalized = normalizeAction(action);
    if (!normalized.ok) {
      return normalized;
    }
    actions.push(normalized.data);
  }

  return ok(actions);
}

export function serializeAction(action: Action): ActionWire {
  switch (action.type) {
    case "end-turn":
    case "co-power":
    case "super-co-power":
      return { type: action.type };

    case "attack":
    case "move-capture":
    case "move-combine":
    case "move-load":
    case "move-wait":
      return {
        type: action.type,
        source: serializeCoordinate(action.source),
        target: serializeCoordinate(action.target),
      };

    case "move-attack":
      return {
        type: action.type,
        source: serializeCoordinate(action.source),
        target: serializeCoordinate(action.target),
        direction: action.direction,
      };

    case "repair":
      return {
        type: action.type,
        source: serializeCoordinate(action.source),
        direction: action.direction,
      };

    case "unload":
      return {
        type: action.type,
        source: serializeCoordinate(action.source),
        direction: action.direction,
        unloadIndex: action.unloadIndex,
      };

    case "buy":
      return {
        type: action.type,
        source: serializeCoordinate(action.source),
        unit: action.unitType,
      };
  }
}
