import type { ActionHighlights } from "./actions";
import { spriteLocations } from "./spriteLocations";
import { terrainSpriteName } from "./terrain";
import { unitAssetPath, unitFallbackLabel } from "./unitAssets";
import type { Coordinate, GameState, MapTile } from "../gameState/schema";

const tileSize = 16;
const plainSprite = spriteLocations.clear.plain;

export type BoardImages = {
  spritesheet?: HTMLImageElement;
  cursor?: HTMLImageElement;
  units: Map<string, HTMLImageElement>;
};

export type RenderBoardOptions = {
  images: BoardImages;
  selected?: Coordinate;
  hover?: Coordinate;
  highlights?: ActionHighlights;
  showCoordinates: boolean;
  showGrid: boolean;
  showTerrainIds: boolean;
};

type SpriteInfo = {
  x: number;
  y: number;
  w: number;
  h: number;
};

function drawTextBadge(
  ctx: CanvasRenderingContext2D,
  text: string,
  x: number,
  y: number,
  color: string,
  align: CanvasTextAlign = "left"
) {
  ctx.save();
  ctx.font = "8px 'Trebuchet MS', Arial, sans-serif";
  ctx.textBaseline = "top";
  ctx.textAlign = align;
  const metrics = ctx.measureText(text);
  const width = Math.ceil(metrics.width) + 3;
  const left = align === "right" ? x - width : x;
  ctx.fillStyle = "rgba(16, 20, 22, 0.74)";
  ctx.fillRect(left, y, width, 9);
  ctx.fillStyle = color;
  ctx.fillText(text, x + (align === "right" ? -2 : 2), y + 1);
  ctx.restore();
}

function drawPlain(ctx: CanvasRenderingContext2D, spritesheet: HTMLImageElement, col: number, row: number) {
  ctx.drawImage(
    spritesheet,
    plainSprite.x,
    plainSprite.y,
    plainSprite.w,
    plainSprite.h,
    col * tileSize,
    row * tileSize,
    tileSize,
    tileSize
  );
}

function drawFallbackTile(ctx: CanvasRenderingContext2D, tile: MapTile, col: number, row: number) {
  ctx.fillStyle = "#91b45b";
  ctx.fillRect(col * tileSize, row * tileSize, tileSize, tileSize);
  drawTextBadge(ctx, String(tile.terrain), col * tileSize + 1, row * tileSize + 4, "#fff5a4");
}

const riverSouth = new Set([5, 6, 7, 8, 11, 12, 14]);
const riverEast = new Set([4, 6, 7, 10, 11, 13, 14]);
const riverWest = new Set([4, 6, 8, 9, 11, 12, 13]);
const riverNorth = new Set([5, 6, 9, 10, 12, 13, 14]);

function seaSprite(map: MapTile[][], x: number, y: number): SpriteInfo | undefined {
  const rows = map.length - 1;
  const cols = map[0].length - 1;
  let total = 0;
  const border = [
    [x - 1, y - 1],
    [x, y - 1],
    [x + 1, y - 1],
    [x + 1, y],
    [x + 1, y + 1],
    [x, y + 1],
    [x - 1, y + 1],
    [x - 1, y]
  ];

  for (let k = 0; k <= 7; k += 1) {
    const [xTest, yTest] = border[k];
    const id = xTest >= 0 && xTest <= cols && yTest >= 0 && yTest <= rows ? map[yTest][xTest].terrain : 32;

    if ((id >= 26 && id <= 33) || id === 195) {
      continue;
    }

    if (id >= 4 && id <= 14) {
      if (k === 1 && riverSouth.has(id)) {
        total |= 0x05;
      } else if (k === 3 && riverWest.has(id)) {
        total |= 0x14;
      } else if (k === 5 && riverNorth.has(id)) {
        total |= 0x50;
      } else if (k === 7 && riverEast.has(id)) {
        total |= 0x41;
      } else {
        total |= 1 << k;
      }
    } else {
      total |= 1 << k;
    }
  }

  total &= ~(((total << 1) | (total >> 1) | (total >> 7)) & 0x55);
  return spriteLocations.clear[`sea${total}`];
}

function shoalSprite(map: MapTile[][], x: number, y: number): SpriteInfo | undefined {
  const rows = map.length - 1;
  const cols = map[0].length - 1;
  let total = 0;
  const border = [
    [x, y - 1],
    [x - 1, y],
    [x + 1, y],
    [x, y + 1]
  ];

  for (let k = 0; k <= 3; k += 1) {
    const [xTest, yTest] = border[k];
    const inBounds = xTest >= 0 && xTest <= cols && yTest >= 0 && yTest <= rows;
    const id = inBounds ? map[yTest][xTest].terrain : 0;
    let value = 2;

    if (!inBounds) {
      value = 0;
    } else if (id === 28 || id === 33) {
      value = 0;
    } else if (id >= 4 && id <= 14) {
      value = 2;
      if (k === 0 && riverSouth.has(id)) {
        value = 1;
      } else if (k === 1 && riverEast.has(id)) {
        value = 1;
      } else if (k === 2 && riverWest.has(id)) {
        value = 1;
      } else if (k === 3 && riverNorth.has(id)) {
        value = 1;
      }
    } else if (id === 26 && (k === 0 || k === 3)) {
      value = 1;
    } else if (id === 27 && (k === 1 || k === 2)) {
      value = 1;
    } else if ((id >= 29 && id <= 32) || id === 195) {
      value = 1;
    }

    total += 3 ** k * value;
  }

  return spriteLocations.clear[`shoal${total}`];
}

function resolveTerrainSprite(map: MapTile[][], col: number, row: number): SpriteInfo | undefined {
  const tile = map[row][col];
  const name = terrainSpriteName(tile.terrain);
  if (!name) {
    return undefined;
  }

  if (name === "sea") {
    return seaSprite(map, col, row);
  }

  if (name === "hshoal" || name === "hshoaln" || name === "vshoal" || name === "vshoale") {
    return shoalSprite(map, col, row);
  }

  return spriteLocations.clear[name];
}

function drawTerrain(ctx: CanvasRenderingContext2D, state: GameState, images: BoardImages) {
  const spritesheet = images.spritesheet;
  if (!spritesheet) {
    return;
  }

  for (let row = 0; row < state.map.length; row += 1) {
    for (let col = 0; col < state.map[row].length; col += 1) {
      const tile = state.map[row][col];
      const sprite = resolveTerrainSprite(state.map, col, row);
      if (!sprite) {
        drawFallbackTile(ctx, tile, col, row);
        continue;
      }

      if (tile.terrain > 33 || tile.terrain === 2 || tile.terrain === 3) {
        drawPlain(ctx, spritesheet, col, row);
      }

      ctx.drawImage(
        spritesheet,
        sprite.x,
        sprite.y,
        sprite.w,
        sprite.h,
        col * tileSize,
        row * tileSize - (sprite.h - tileSize),
        sprite.w,
        sprite.h
      );
    }
  }
}

function drawUnits(ctx: CanvasRenderingContext2D, state: GameState, images: BoardImages) {
  for (let row = 0; row < state.map.length; row += 1) {
    for (let col = 0; col < state.map[row].length; col += 1) {
      const unit = state.map[row][col].unit;
      if (!unit) {
        continue;
      }

      const assetPath = unitAssetPath(unit.owner, unit.type, unit.moved);
      const image = assetPath ? images.units.get(assetPath) : undefined;
      const x = col * tileSize;
      const y = row * tileSize;

      if (image) {
        ctx.drawImage(image, x, y);
      } else {
        ctx.fillStyle = unit.owner === "blue-moon" ? "#2b67d1" : "#d86f1f";
        ctx.fillRect(x + 2, y + 2, tileSize - 4, tileSize - 4);
        drawTextBadge(ctx, unitFallbackLabel(unit.type), x + 1, y + 4, "#fff");
      }

      const hp = Math.max(1, Math.ceil(unit.health / 10));
      drawTextBadge(ctx, String(hp), x, y + 8, hp <= 3 ? "#ffdf69" : "#f8f8f8");

      if (unit.fuel <= 10) {
        ctx.fillStyle = "#f5c542";
        ctx.fillRect(x + 11, y + 1, 4, 4);
      }

      if (unit.ammo === 0) {
        ctx.fillStyle = "#ec5151";
        ctx.fillRect(x + 11, y + 6, 4, 4);
      }

      if (unit["loaded-units"]?.length) {
        drawTextBadge(ctx, String(unit["loaded-units"].length), x + 15, y + 8, "#b8f7ff", "right");
      }
    }
  }
}

function drawCaptureProgress(ctx: CanvasRenderingContext2D, state: GameState) {
  for (let row = 0; row < state.map.length; row += 1) {
    for (let col = 0; col < state.map[row].length; col += 1) {
      const property = state.map[row][col].property;
      if (!property || property["capture-points"] >= 20) {
        continue;
      }

      const x = col * tileSize;
      const y = row * tileSize + 13;
      ctx.fillStyle = "rgba(35, 23, 16, 0.76)";
      ctx.fillRect(x + 1, y, 14, 2);
      ctx.fillStyle = "#ffd54a";
      ctx.fillRect(x + 1, y, Math.max(1, Math.round((property["capture-points"] / 20) * 14)), 2);
    }
  }
}

function drawHighlights(ctx: CanvasRenderingContext2D, options: RenderBoardOptions) {
  if (!options.highlights) {
    return;
  }

  ctx.save();
  for (const move of options.highlights.moves) {
    ctx.fillStyle = "rgba(52, 128, 255, 0.36)";
    ctx.fillRect(move.x * tileSize, move.y * tileSize, tileSize, tileSize);
  }

  for (const attack of options.highlights.attacks) {
    ctx.fillStyle = "rgba(237, 53, 53, 0.42)";
    ctx.fillRect(attack.x * tileSize, attack.y * tileSize, tileSize, tileSize);
  }
  ctx.restore();
}

function drawDebugOverlays(ctx: CanvasRenderingContext2D, state: GameState, options: RenderBoardOptions) {
  for (let row = 0; row < state.map.length; row += 1) {
    for (let col = 0; col < state.map[row].length; col += 1) {
      const x = col * tileSize;
      const y = row * tileSize;

      if (options.showGrid) {
        ctx.strokeStyle = "rgba(27, 35, 41, 0.24)";
        ctx.strokeRect(x + 0.5, y + 0.5, tileSize, tileSize);
      }

      if (options.showCoordinates) {
        drawTextBadge(ctx, `${col},${row}`, x + 1, y + 1, "#f4fbff");
      }

      if (options.showTerrainIds) {
        drawTextBadge(ctx, String(state.map[row][col].terrain), x + 1, y + 8, "#fff5a4");
      }
    }
  }
}

function drawCursor(ctx: CanvasRenderingContext2D, options: RenderBoardOptions) {
  const cursor = options.images.cursor;
  const selected = options.selected ?? options.hover;
  if (!selected) {
    return;
  }

  if (cursor) {
    ctx.drawImage(cursor, selected[0] * tileSize - 6, selected[1] * tileSize - 6);
    return;
  }

  ctx.strokeStyle = "#fff";
  ctx.lineWidth = 1;
  ctx.strokeRect(selected[0] * tileSize + 0.5, selected[1] * tileSize + 0.5, tileSize - 1, tileSize - 1);
}

export function renderBoard(ctx: CanvasRenderingContext2D, state: GameState, options: RenderBoardOptions) {
  const width = state.map[0]?.length ?? 0;
  const height = state.map.length;
  ctx.clearRect(0, 0, width * tileSize, height * tileSize);
  ctx.imageSmoothingEnabled = false;

  drawTerrain(ctx, state, options.images);
  drawHighlights(ctx, options);
  drawCaptureProgress(ctx, state);
  drawUnits(ctx, state, options.images);
  drawDebugOverlays(ctx, state, options);
  drawCursor(ctx, options);
}

export const boardTileSize = tileSize;
