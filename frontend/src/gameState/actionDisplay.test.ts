import { describe, expect, it } from "vitest";
import type { Action, Player } from "./schema";
import { actionToSubmitFromBoardTarget, actionsVisibleInInspector, buyMenuItems, globalActionsFromLegalActions, labelForAction } from "./actionDisplay";

const activePlayer: Player = {
  armyType: "orange-star",
  co: "Andy",
  funds: 50_000,
  "power-meter": {
    charge: 0,
    "cop-stars": 3,
    "scop-stars": 3,
    "star-value": 9000
  },
  "power-status": 0,
  "luck-policy": 1
};

describe("globalActionsFromLegalActions", () => {
  it("keeps source-less actions available apart from tile actions", () => {
    const endTurn: Action = { type: "end-turn" };
    const tileAction: Action = { type: "move-wait", source: [1, 1], target: [1, 1] };

    expect(globalActionsFromLegalActions([tileAction, endTurn])).toEqual([endTurn]);
  });
});

describe("actionsVisibleInInspector", () => {
  it("shows global actions when the selected tile has none", () => {
    const endTurn: Action = { type: "end-turn" };

    expect(
      actionsVisibleInInspector({
        globalActions: [endTurn],
        inspectedActions: [],
        selectedActions: []
      })
    ).toEqual([endTurn]);
  });

  it("uses inspected target actions while hovering a highlighted target", () => {
    const endTurn: Action = { type: "end-turn" };
    const attack: Action = { type: "attack", source: [1, 1], target: [2, 1] };

    expect(
      actionsVisibleInInspector({
        globalActions: [endTurn],
        inspectedActions: [attack],
        selectedActions: []
      })
    ).toEqual([attack]);
  });
});

describe("actionToSubmitFromBoardTarget", () => {
  it("confirms a single movement action from a board target click", () => {
    const move: Action = { type: "move-wait", source: [1, 1], target: [2, 1] };

    expect(actionToSubmitFromBoardTarget([move])).toBe(move);
  });

  it("confirms hidden-state move actions from a board target click", () => {
    const hide: Action = { type: "move-hide", source: [1, 1], target: [2, 1] };
    const unhide: Action = { type: "move-unhide", source: [1, 1], target: [1, 1] };

    expect(actionToSubmitFromBoardTarget([hide])).toBe(hide);
    expect(actionToSubmitFromBoardTarget([unhide])).toBe(unhide);
  });

  it("leaves attack target clicks for the inspector", () => {
    const attack: Action = { type: "attack", source: [1, 1], target: [3, 1] };

    expect(actionToSubmitFromBoardTarget([attack])).toBeUndefined();
  });

  it("does not choose from ambiguous target actions", () => {
    expect(
      actionToSubmitFromBoardTarget([
        { type: "move-wait", source: [1, 1], target: [2, 1] },
        { type: "move-load", source: [1, 1], target: [2, 1] }
      ])
    ).toBeUndefined();
  });
});

describe("labelForAction", () => {
  it("uses a readable command label for ending the turn", () => {
    expect(labelForAction({ type: "end-turn" })).toBe("End turn");
  });
});

describe("buyMenuItems", () => {
  it("turns buy actions into sorted deployment menu rows", () => {
    const tank: Action = { type: "buy", source: [1, 1], unit: "tank" };
    const infantry: Action = { type: "buy", source: [1, 1], unit: "infantry" };
    const mediumTank: Action = { type: "buy", source: [1, 1], unit: "medium-tank" };

    expect(buyMenuItems([tank, mediumTank, infantry], activePlayer)).toEqual([
      { action: infantry, unit: "infantry", label: "Infantry", cost: 1000 },
      { action: tank, unit: "tank", label: "Tank", cost: 7000 },
      { action: mediumTank, unit: "medium-tank", label: "Md.Tank", cost: 16000 }
    ]);
  });

  it("uses active CO build costs", () => {
    const hachi: Player = {
      ...activePlayer,
      co: "Hachi",
      "power-status": 2
    };

    expect(buyMenuItems([{ type: "buy", source: [1, 1], unit: "mech" }], hachi)[0].cost).toBe(1500);
  });
});
