import type { Action, Player } from "./schema";

type InspectorActionSets = {
  globalActions: Action[];
  inspectedActions: Action[];
  selectedActions: Action[];
};

export type BuyMenuItem = {
  action: Action;
  unit: string;
  label: string;
  cost: number;
};

const unitBaseCosts: Record<string, number> = {
  "anti-air": 8000,
  apc: 5000,
  artillery: 6000,
  battleship: 28000,
  bcopter: 9000,
  blackboat: 7500,
  blackbomb: 25000,
  bomber: 22000,
  carrier: 30000,
  crusier: 18000,
  fighter: 20000,
  infantry: 1000,
  lander: 12000,
  "medium-tank": 16000,
  mech: 3000,
  megatank: 28000,
  missile: 12000,
  neotank: 22000,
  piperunner: 20000,
  recon: 4000,
  rocket: 15000,
  stealth: 24000,
  sub: 20000,
  tank: 7000,
  tcopter: 5000
};

const unitLabels: Record<string, string> = {
  "anti-air": "Anti-Air",
  apc: "APC",
  artillery: "Artillery",
  battleship: "Battleship",
  bcopter: "B-Copter",
  blackboat: "Black Boat",
  blackbomb: "Black Bomb",
  bomber: "Bomber",
  carrier: "Carrier",
  crusier: "Cruiser",
  fighter: "Fighter",
  infantry: "Infantry",
  lander: "Lander",
  "medium-tank": "Md.Tank",
  mech: "Mech",
  megatank: "Mega Tank",
  missile: "Missile",
  neotank: "Neotank",
  piperunner: "Piperunner",
  recon: "Recon",
  rocket: "Rocket",
  stealth: "Stealth",
  sub: "Sub",
  tank: "Tank",
  tcopter: "T-Copter"
};

const unitDisplayOrder = [
  "infantry",
  "mech",
  "recon",
  "apc",
  "artillery",
  "tank",
  "anti-air",
  "missile",
  "rocket",
  "medium-tank",
  "piperunner",
  "neotank",
  "megatank",
  "tcopter",
  "bcopter",
  "fighter",
  "bomber",
  "stealth",
  "blackbomb",
  "lander",
  "blackboat",
  "crusier",
  "sub",
  "battleship",
  "carrier"
];

const unitOrderIndex = new Map(unitDisplayOrder.map((unit, index) => [unit, index]));

const boardClickConfirmActionTypes = new Set([
  "move-wait",
  "move-capture",
  "move-load",
  "move-combine"
]);

function labelFromType(type: string): string {
  return type
    .split("-")
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
    .join(" ");
}

function normalizedCoName(player: Player): string {
  return player.co.toLowerCase().replace(/[^a-z]/g, "");
}

function buildCostForPlayer(unit: string, player: Player): number | undefined {
  const baseCost = unitBaseCosts[unit];
  if (baseCost === undefined) {
    return undefined;
  }

  const co = normalizedCoName(player);
  if (co === "colin") {
    return Math.trunc((baseCost * 80) / 100);
  }

  if (co === "hachi") {
    return Math.trunc((baseCost * (player["power-status"] === 0 ? 90 : 50)) / 100);
  }

  if (co === "kanbei") {
    return Math.trunc((baseCost * 120) / 100);
  }

  return baseCost;
}

export function globalActionsFromLegalActions(actions: Action[]): Action[] {
  return actions.filter((action) => action.source === undefined);
}

export function actionsVisibleInInspector({
  globalActions,
  inspectedActions,
  selectedActions
}: InspectorActionSets): Action[] {
  if (inspectedActions.length > 0) {
    return inspectedActions;
  }

  return [...globalActions, ...selectedActions];
}

export function actionToSubmitFromBoardTarget(actions: Action[]): Action | undefined {
  if (actions.length !== 1) {
    return undefined;
  }

  const [action] = actions;
  return boardClickConfirmActionTypes.has(action.type) ? action : undefined;
}

export function labelForAction(action: Action): string {
  if (action.type === "end-turn") {
    return "End turn";
  }

  if (action.type === "co-power") {
    return "CO power";
  }

  if (action.type === "super-co-power") {
    return "Super CO power";
  }

  return labelFromType(action.type);
}

export function buyMenuItems(actions: Action[], player: Player | undefined): BuyMenuItem[] {
  if (!player) {
    return [];
  }

  return actions
    .filter((action) => action.type === "buy" && action.unit !== undefined)
    .map((action) => {
      const unit = action.unit ?? "";
      const cost = buildCostForPlayer(unit, player);
      if (cost === undefined) {
        return undefined;
      }

      return {
        action,
        unit,
        label: unitLabels[unit] ?? labelFromType(unit),
        cost
      };
    })
    .filter((item): item is BuyMenuItem => item !== undefined)
    .sort((a, b) => {
      const orderA = unitOrderIndex.get(a.unit) ?? Number.MAX_SAFE_INTEGER;
      const orderB = unitOrderIndex.get(b.unit) ?? Number.MAX_SAFE_INTEGER;
      if (orderA !== orderB) {
        return orderA - orderB;
      }

      return a.label.localeCompare(b.label);
    });
}
