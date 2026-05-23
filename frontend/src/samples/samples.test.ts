import { describe, expect, it } from "vitest";

import basicGameState from "../../samples/wire/current/basic-game-state.json";
import legalActions from "../../samples/wire/current/legal-actions.json";
import { parseActions } from "../adapter/actions";
import { parseGameState } from "../adapter/game-state";

describe("curated samples", () => {
  it("parses the current basic game-state sample", () => {
    const result = parseGameState(basicGameState);

    expect(result.ok).toBe(true);
    if (!result.ok) {
      return;
    }
    expect(result.data.board.width).toBe(2);
    expect(result.data.players).toHaveLength(2);
  });

  it("parses the current legal-actions sample", () => {
    const result = parseActions(legalActions);

    expect(result.ok).toBe(true);
    if (!result.ok) {
      return;
    }
    expect(result.data.map((action) => action.type)).toEqual([
      "move-attack",
      "buy",
      "end-turn",
    ]);
  });
});
