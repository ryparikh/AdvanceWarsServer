import { useEffect, useMemo, useRef, useState } from "react";
import { movementTrailAfterHover, type ActionHighlights } from "../rendering/actions";
import { boardTileSize, type BoardImages, renderBoard } from "../rendering/renderBoard";
import { unitAssetPath } from "../rendering/unitAssets";
import type { Coordinate, GameState } from "../gameState/schema";

type BoardCanvasProps = {
  gameState: GameState;
  selected?: Coordinate;
  highlights?: ActionHighlights;
  zoom: number;
  showCoordinates: boolean;
  showGrid: boolean;
  showTerrainIds: boolean;
  changedTiles?: Coordinate[];
  onSelectTile: (coordinate: Coordinate) => void;
};

function loadImage(src: string): Promise<HTMLImageElement | undefined> {
  return new Promise((resolve) => {
    const image = new Image();
    image.onload = () => resolve(image);
    image.onerror = () => resolve(undefined);
    image.src = src;
  });
}

function unitAssetPaths(gameState: GameState): string[] {
  const paths = new Set<string>();
  for (const row of gameState.map) {
    for (const tile of row) {
      if (tile.unit) {
        const assetPath = unitAssetPath(tile.unit.owner, tile.unit.type, tile.unit.moved);
        if (assetPath) {
          paths.add(assetPath);
        }
      }
    }
  }
  return [...paths];
}

export function BoardCanvas({
  gameState,
  selected,
  highlights,
  zoom,
  showCoordinates,
  showGrid,
  showTerrainIds,
  changedTiles,
  onSelectTile
}: BoardCanvasProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [images, setImages] = useState<BoardImages>({ units: new Map() });
  const [hover, setHover] = useState<Coordinate | undefined>();
  const [movementTrail, setMovementTrail] = useState<Coordinate[] | undefined>();
  const assetPaths = useMemo(() => unitAssetPaths(gameState), [gameState]);
  const cols = gameState.map[0]?.length ?? 0;
  const rows = gameState.map.length;
  const cssWidth = cols * boardTileSize * zoom;
  const cssHeight = rows * boardTileSize * zoom;

  useEffect(() => {
    let alive = true;

    async function loadImages() {
      const [spritesheet, cursor, ...unitImages] = await Promise.all([
        loadImage("/assets/spritesheet.png"),
        loadImage("/assets/unit_select.gif"),
        ...assetPaths.map((path) => loadImage(path))
      ]);

      if (!alive) {
        return;
      }

      const units = new Map<string, HTMLImageElement>();
      assetPaths.forEach((path, index) => {
        const image = unitImages[index];
        if (image) {
          units.set(path, image);
        }
      });
      setImages({ spritesheet, cursor, units });
    }

    loadImages();
    return () => {
      alive = false;
    };
  }, [assetPaths]);

  useEffect(() => {
    setMovementTrail(selected ? [selected] : undefined);
  }, [highlights, selected]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) {
      return;
    }

    const ratio = window.devicePixelRatio || 1;
    canvas.width = Math.max(1, Math.floor(cssWidth * ratio));
    canvas.height = Math.max(1, Math.floor(cssHeight * ratio));
    canvas.style.width = `${cssWidth}px`;
    canvas.style.height = `${cssHeight}px`;

    const ctx = canvas.getContext("2d");
    if (!ctx) {
      return;
    }

    ctx.setTransform(ratio * zoom, 0, 0, ratio * zoom, 0, 0);
    renderBoard(ctx, gameState, {
      images,
      selected,
      hover,
      movementTrail,
      highlights,
      changedTiles,
      showCoordinates,
      showGrid,
      showTerrainIds
    });
  }, [changedTiles, cssHeight, cssWidth, gameState, highlights, hover, images, movementTrail, selected, showCoordinates, showGrid, showTerrainIds, zoom]);

  function eventToCoordinate(event: React.MouseEvent<HTMLCanvasElement>): Coordinate | undefined {
    const rect = event.currentTarget.getBoundingClientRect();
    const x = Math.floor((event.clientX - rect.left) / (boardTileSize * zoom));
    const y = Math.floor((event.clientY - rect.top) / (boardTileSize * zoom));
    if (x < 0 || y < 0 || y >= rows || x >= cols) {
      return undefined;
    }
    return [x, y];
  }

  return (
    <canvas
      ref={canvasRef}
      className="board-canvas"
      onPointerLeave={() => setHover(undefined)}
      onPointerMove={(event) => {
        const coordinate = eventToCoordinate(event);
        setHover(coordinate);
        setMovementTrail((current) => movementTrailAfterHover(current, selected, coordinate, highlights));
      }}
      onClick={(event) => {
        const coordinate = eventToCoordinate(event);
        if (coordinate) {
          setHover(coordinate);
          setMovementTrail((current) => movementTrailAfterHover(current, selected, coordinate, highlights));
          onSelectTile(coordinate);
        }
      }}
    />
  );
}
