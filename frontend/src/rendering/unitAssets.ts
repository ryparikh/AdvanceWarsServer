const ownerAssetDirectories = new Set(["orange-star", "blue-moon"]);

const unitAssetNames: Record<string, string> = {
  bcopter: "b-copter",
  tcopter: "t-copter",
  "medium-tank": "mdtank",
  crusier: "cruiser"
};

const supportedUnitAssetNames = new Set([
  "anti-air",
  "apc",
  "artillery",
  "b-copter",
  "battleship",
  "blackboat",
  "blackbomb",
  "bomber",
  "carrier",
  "cruiser",
  "fighter",
  "infantry",
  "lander",
  "mdtank",
  "mech",
  "megatank",
  "missile",
  "neotank",
  "piperunner",
  "recon",
  "rocket",
  "stealth",
  "sub",
  "t-copter",
  "tank"
]);

export function unitAssetName(unitType: string): string | undefined {
  const assetName = unitAssetNames[unitType] ?? unitType;
  return supportedUnitAssetNames.has(assetName) ? assetName : undefined;
}

export function unitAssetPath(owner: string, unitType: string, moved: boolean): string | undefined {
  if (!ownerAssetDirectories.has(owner)) {
    return undefined;
  }

  const assetName = unitAssetName(unitType);
  if (!assetName) {
    return undefined;
  }

  const suffix = moved ? "_moved" : "";
  return `/assets/${owner}/${assetName}${suffix}.gif`;
}

export function unitFallbackLabel(unitType: string): string {
  const assetName = unitAssetNames[unitType] ?? unitType;
  return assetName.replace(/[^a-z0-9-]/gi, "").slice(0, 2).toUpperCase() || "??";
}
