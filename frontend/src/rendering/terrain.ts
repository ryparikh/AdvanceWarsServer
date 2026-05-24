import { TS_terrainIdToName } from "./spriteLocations";

const displayNames: Record<string, string> = {
  criver: "River",
  croad: "Road",
  espipe: "Pipe",
  esriver: "River",
  esroad: "Road",
  eswriver: "River",
  eswroad: "Road",
  hbridge: "Bridge",
  hpipe: "Pipe",
  hpiperubble: "Pipe Rubble",
  hpipeseam: "Pipe Seam",
  hriver: "River",
  hroad: "Road",
  hshoal: "Shoal",
  hshoaln: "Shoal",
  nepipe: "Pipe",
  neriver: "River",
  neroad: "Road",
  nesriver: "River",
  nesroad: "Road",
  npipeend: "Pipe End",
  plain: "Plain",
  spipeend: "Pipe End",
  swpipe: "Pipe",
  swriver: "River",
  swroad: "Road",
  swnriver: "River",
  swnroad: "Road",
  vbridge: "Bridge",
  vpipe: "Pipe",
  vpiperubble: "Pipe Rubble",
  vpipeseam: "Pipe Seam",
  vriver: "River",
  vroad: "Road",
  vshoal: "Shoal",
  vshoale: "Shoal",
  wnpipe: "Pipe",
  wneriver: "River",
  wneroad: "Road",
  wnriver: "River",
  wnroad: "Road",
  wpipeend: "Pipe End"
};

const armyDisplayNames: Record<string, string> = {
  acidrain: "Acid Rain",
  amberblaze: "Amber Blaze",
  azureasteroid: "Azure Asteroid",
  blackhole: "Black Hole",
  bluemoon: "Blue Moon",
  browndesert: "Brown Desert",
  cobaltice: "Cobalt Ice",
  greenearth: "Green Earth",
  greysky: "Grey Sky",
  jadesun: "Jade Sun",
  neutral: "Neutral",
  noireclipse: "Noir Eclipse",
  orangestar: "Orange Star",
  pinkcosmos: "Pink Cosmos",
  purplelightning: "Purple Lightning",
  redfire: "Red Fire",
  tealgalaxy: "Teal Galaxy",
  whitenova: "White Nova",
  yellowcomet: "Yellow Comet"
};

const propertySuffixes = ["airport", "base", "city", "comtower", "hq", "lab", "port"] as const;

function titleCase(value: string): string {
  return value
    .replace(/([a-z])([A-Z])/g, "$1 $2")
    .replace(/[-_]/g, " ")
    .replace(/\b\w/g, (letter) => letter.toUpperCase());
}

function terrainLabel(spriteName: string): string {
  const directName = displayNames[spriteName];
  if (directName) {
    return directName;
  }

  for (const suffix of propertySuffixes) {
    if (!spriteName.endsWith(suffix)) {
      continue;
    }

    const armyName = spriteName.slice(0, -suffix.length);
    const displayArmy = armyDisplayNames[armyName] ?? titleCase(armyName);
    const displayProperty = suffix === "hq" ? "HQ" : titleCase(suffix);
    return `${displayArmy} ${displayProperty}`;
  }

  return titleCase(spriteName);
}

export function terrainSpriteName(terrainId: number): string | undefined {
  return TS_terrainIdToName[String(terrainId)];
}

export function terrainDisplayName(terrainId: number): string {
  const spriteName = terrainSpriteName(terrainId);
  if (!spriteName) {
    return `Unknown (${terrainId})`;
  }

  return `${terrainLabel(spriteName)} (${terrainId})`;
}
