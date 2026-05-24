import { useEffect, useMemo, useRef, useState } from "react";
import { BoardCanvas } from "./components/BoardCanvas";
import { actionHighlightsForSource } from "./rendering/actions";
import { actionToSubmitFromBoardTarget, actionsVisibleInInspector, buyMenuItems, globalActionsFromLegalActions, labelForAction } from "./gameState/actionDisplay";
import { coordinateKey, parseGameStatePayload, type Action, legalActionEnvelopeSchema, type Coordinate, type GameState, type ValidActionGroup } from "./gameState/schema";
import { normalizeServerBaseUrl, serverApiUrl } from "./api/url";
import { terrainDisplayName } from "./rendering/terrain";
import { unitAssetPath, unitFallbackLabel } from "./rendering/unitAssets";
import "./styles.css";

const defaultServerBaseUrl = "http://localhost:80";
const storageKey = "advance-wars-server-base-url";
const maxPastedJsonCharacters = 2_000_000;

type LoadSource = "sample" | "server" | "pasted";

type LoadedState = {
  gameState: GameState;
  globalActions: Action[];
  legalActionGroups: ValidActionGroup[];
  source: LoadSource;
  label: string;
};

type Toggles = {
  coordinates: boolean;
  grid: boolean;
  terrainIds: boolean;
};

function storedServerBaseUrl(): string {
  try {
    return localStorage.getItem(storageKey) ?? defaultServerBaseUrl;
  } catch {
    return defaultServerBaseUrl;
  }
}

function rememberServerBaseUrl(url: string) {
  try {
    localStorage.setItem(storageKey, url);
  } catch {
    // Browser storage can be disabled; the input value still works for this session.
  }
}

async function fetchJson(path: string): Promise<unknown> {
  const response = await fetch(path);
  if (!response.ok) {
    throw new Error(`${response.status} ${response.statusText}`);
  }
  return response.json();
}

async function fetchServerGlobalActions(baseUrl: string, gameId: string): Promise<Action[]> {
  const response = await fetch(serverApiUrl(baseUrl, "games", gameId, "actions"), {
    credentials: "omit"
  });
  if (!response.ok) {
    throw new Error(`${response.status} ${response.statusText}`);
  }

  const envelope = legalActionEnvelopeSchema.parse(await response.json());
  return globalActionsFromLegalActions(envelope.actions);
}

function actionGroupForSelection(groups: ValidActionGroup[], selected?: Coordinate): Action[] {
  if (!selected) {
    return [];
  }
  return groups.find((group) => group.source[0] === selected[0] && group.source[1] === selected[1])?.expected ?? [];
}

function formatMoney(value: number): string {
  return value.toLocaleString("en-US");
}

function activePlayerName(gameState: GameState): string {
  const player = gameState.players[gameState.activePlayer];
  if (!player) {
    return `Player ${gameState.activePlayer + 1}`;
  }
  return `${player.armyType} / ${player.co}`;
}

function selectedTile(gameState: GameState, selected?: Coordinate) {
  if (!selected) {
    return undefined;
  }
  return gameState.map[selected[1]]?.[selected[0]];
}

export default function App() {
  const [loaded, setLoaded] = useState<LoadedState | undefined>();
  const [selected, setSelected] = useState<Coordinate | undefined>();
  const [selectedActions, setSelectedActions] = useState<Action[]>([]);
  const [serverBaseUrl, setServerBaseUrl] = useState(storedServerBaseUrl);
  const [pasteValue, setPasteValue] = useState("");
  const [error, setError] = useState<string | undefined>();
  const [isLoading, setIsLoading] = useState(false);
  const [zoom, setZoom] = useState(2.4);
  const [inspectedCoordinate, setInspectedCoordinate] = useState<Coordinate | undefined>();
  const [inspectedActions, setInspectedActions] = useState<Action[]>([]);
  const loadRequestId = useRef(0);
  const actionRequestId = useRef(0);
  const [toggles, setToggles] = useState<Toggles>({
    coordinates: false,
    grid: false,
    terrainIds: false
  });

  async function loadPayload(path: string, label: string) {
    const requestId = loadRequestId.current + 1;
    loadRequestId.current = requestId;
    setIsLoading(true);
    setError(undefined);
    try {
      const payload = await fetchJson(path);
      const parsed = parseGameStatePayload(payload);
      if (loadRequestId.current !== requestId) {
        return;
      }
      actionRequestId.current += 1;
      setLoaded({ ...parsed, globalActions: [], source: "sample", label });
      setSelected(undefined);
      setInspectedCoordinate(undefined);
      setSelectedActions([]);
      setInspectedActions([]);
    } catch (err) {
      if (loadRequestId.current === requestId) {
        setError(err instanceof Error ? err.message : "Unable to load sample");
      }
    } finally {
      if (loadRequestId.current === requestId) {
        setIsLoading(false);
      }
    }
  }

  useEffect(() => {
    loadPayload("/samples/lefty.json", "Lefty sample");
  }, []);

  const highlights = useMemo(() => {
    if (!selected || selectedActions.length === 0) {
      return undefined;
    }
    return actionHighlightsForSource(selectedActions, selected);
  }, [selected, selectedActions]);

  async function createServerGame() {
    const requestId = loadRequestId.current + 1;
    loadRequestId.current = requestId;
    setIsLoading(true);
    setError(undefined);
    try {
      const baseUrl = normalizeServerBaseUrl(serverBaseUrl);
      rememberServerBaseUrl(baseUrl);
      setServerBaseUrl(baseUrl);
      const response = await fetch(serverApiUrl(baseUrl, "games"), {
        method: "POST",
        credentials: "omit",
        headers: {
          "Content-Type": "application/json"
        },
        body: JSON.stringify({
          mapId: "lefty",
          players: [
            { co: "andy", armyType: "orange-star" },
            { co: "adder", armyType: "blue-moon" }
          ]
        })
      });
      if (!response.ok) {
        throw new Error(`${response.status} ${response.statusText}`);
      }
      const parsed = parseGameStatePayload(await response.json());
      const globalActions = await fetchServerGlobalActions(baseUrl, parsed.gameState.gameId);
      if (loadRequestId.current !== requestId) {
        return;
      }
      actionRequestId.current += 1;
      setLoaded({ ...parsed, globalActions, source: "server", label: "Server game" });
      setSelected(undefined);
      setInspectedCoordinate(undefined);
      setSelectedActions([]);
      setInspectedActions([]);
    } catch (err) {
      if (loadRequestId.current === requestId) {
        setError(err instanceof Error ? err.message : "Unable to create server game");
      }
    } finally {
      if (loadRequestId.current === requestId) {
        setIsLoading(false);
      }
    }
  }

  function loadPastedState() {
    loadRequestId.current += 1;
    setIsLoading(false);
    setError(undefined);
    try {
      if (pasteValue.length > maxPastedJsonCharacters) {
        throw new Error("Pasted JSON is too large");
      }
      const parsed = parseGameStatePayload(JSON.parse(pasteValue));
      actionRequestId.current += 1;
      setLoaded({ ...parsed, globalActions: [], source: "pasted", label: "Pasted JSON" });
      setSelected(undefined);
      setInspectedCoordinate(undefined);
      setSelectedActions([]);
      setInspectedActions([]);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Unable to parse pasted JSON");
    }
  }

  async function selectTile(coordinate: Coordinate) {
    const targetActions = highlights?.actionsByTile.get(coordinateKey(coordinate));
    if (selected && targetActions) {
      const actionToSubmit = loaded?.source === "server" && loaded.gameState["game-over"] === false
        ? actionToSubmitFromBoardTarget(targetActions)
        : undefined;
      if (actionToSubmit) {
        await submitServerAction(actionToSubmit);
        return;
      }

      setInspectedCoordinate(coordinate);
      setInspectedActions(targetActions);
      return;
    }

    setSelected(coordinate);
    setInspectedCoordinate(undefined);
    setInspectedActions([]);
    const fixtureActions = actionGroupForSelection(loaded?.legalActionGroups ?? [], coordinate);
    setSelectedActions(fixtureActions);
    const requestId = actionRequestId.current + 1;
    actionRequestId.current = requestId;

    if (!loaded || loaded.source !== "server") {
      return;
    }

    setError(undefined);
    try {
      const actionsUrl = new URL(serverApiUrl(serverBaseUrl, "games", loaded.gameState.gameId, "actions"));
      actionsUrl.searchParams.set("x", String(coordinate[0]));
      actionsUrl.searchParams.set("y", String(coordinate[1]));
      const response = await fetch(actionsUrl.toString(), {
        credentials: "omit"
      });
      if (!response.ok) {
        throw new Error(`${response.status} ${response.statusText}`);
      }
      const actions = legalActionEnvelopeSchema.parse(await response.json()).actions;
      if (actionRequestId.current === requestId) {
        setSelectedActions(actions);
      }
    } catch (err) {
      if (actionRequestId.current === requestId) {
        setError(err instanceof Error ? err.message : "Unable to fetch legal actions");
      }
    }
  }

  async function submitServerAction(action: Action) {
    if (!loaded || loaded.source !== "server") {
      return;
    }

    const requestId = loadRequestId.current + 1;
    loadRequestId.current = requestId;
    actionRequestId.current += 1;
    setIsLoading(true);
    setError(undefined);
    try {
      const baseUrl = normalizeServerBaseUrl(serverBaseUrl);
      rememberServerBaseUrl(baseUrl);
      setServerBaseUrl(baseUrl);
      const response = await fetch(serverApiUrl(baseUrl, "games", loaded.gameState.gameId, "actions"), {
        method: "POST",
        credentials: "omit",
        headers: {
          "Content-Type": "application/json"
        },
        body: JSON.stringify(action)
      });
      if (!response.ok) {
        throw new Error(`${response.status} ${response.statusText}`);
      }

      const parsed = parseGameStatePayload(await response.json());
      const globalActions = await fetchServerGlobalActions(baseUrl, parsed.gameState.gameId);
      if (loadRequestId.current !== requestId) {
        return;
      }

      setLoaded({ ...parsed, globalActions, source: "server", label: loaded.label });
      setSelected(undefined);
      setInspectedCoordinate(undefined);
      setSelectedActions([]);
      setInspectedActions([]);
    } catch (err) {
      if (loadRequestId.current === requestId) {
        setError(err instanceof Error ? err.message : "Unable to submit action");
      }
    } finally {
      if (loadRequestId.current === requestId) {
        setIsLoading(false);
      }
    }
  }

  function toggle(key: keyof Toggles) {
    setToggles((current) => ({ ...current, [key]: !current[key] }));
  }

  const displayCoordinate = inspectedCoordinate ?? selected;
  const tile = loaded ? selectedTile(loaded.gameState, displayCoordinate) : undefined;
  const displayedActions = actionsVisibleInInspector({
    globalActions: loaded?.globalActions ?? [],
    inspectedActions,
    selectedActions
  });
  const activePlayer = loaded?.gameState.players[loaded.gameState.activePlayer];
  const buyItems = inspectedActions.length > 0 ? [] : buyMenuItems(selectedActions, activePlayer);
  const buyActionSet = new Set(buyItems.map((item) => item.action));
  const commandActions = displayedActions.filter((action) => !buyActionSet.has(action));
  const canSubmitDisplayedActions = loaded?.source === "server" && loaded.gameState["game-over"] === false;
  const mapRows = loaded?.gameState.map.length ?? 0;
  const mapCols = loaded?.gameState.map[0]?.length ?? 0;

  return (
    <main className="app-shell">
      <header className="top-strip">
        <div className="game-title">Advance Wars Board Viewer</div>
        <div className="turn-line">
          {loaded ? (
            <>
              <span>Day {loaded.gameState["turn-count"]}</span>
              <span>{activePlayerName(loaded.gameState)}</span>
              <span>{loaded.gameState.weather ?? "clear"}</span>
              <span>{loaded.gameState["game-over"] ? `Winner ${loaded.gameState.winner}` : "active"}</span>
            </>
          ) : (
            <span>Loading board</span>
          )}
        </div>
      </header>

      <section className="toolbar">
        <button type="button" onClick={() => loadPayload("/samples/lefty.json", "Lefty sample")}>
          Lefty
        </button>
        <button type="button" onClick={() => loadPayload("/samples/grit-indirect-range-valid-actions.json", "Grit action sample")}>
          Action sample
        </button>
        <button type="button" onClick={createServerGame}>
          Server game
        </button>
        <label className="server-url">
          <span>Server</span>
          <input value={serverBaseUrl} onChange={(event) => setServerBaseUrl(event.target.value)} />
        </label>
        <div className="toolbar-separator" />
        <button type="button" onClick={() => setZoom((value) => Math.max(1, value - 0.4))}>
          -
        </button>
        <span className="zoom-readout">{zoom.toFixed(1)}x</span>
        <button type="button" onClick={() => setZoom((value) => Math.min(5, value + 0.4))}>
          +
        </button>
        <label>
          <input type="checkbox" checked={toggles.coordinates} onChange={() => toggle("coordinates")} />
          XY
        </label>
        <label>
          <input type="checkbox" checked={toggles.grid} onChange={() => toggle("grid")} />
          Grid
        </label>
        <label>
          <input type="checkbox" checked={toggles.terrainIds} onChange={() => toggle("terrainIds")} />
          IDs
        </label>
      </section>

      {error && <div className="error-line">{error}</div>}
      {isLoading && <div className="loading-line">Loading...</div>}

      <section className="workspace">
        <div className="board-column">
          <div className="board-wrap">
            {loaded && (
              <BoardCanvas
                gameState={loaded.gameState}
                selected={selected}
                highlights={highlights}
                zoom={zoom}
                showCoordinates={toggles.coordinates}
                showGrid={toggles.grid}
                showTerrainIds={toggles.terrainIds}
                onSelectTile={selectTile}
              />
            )}
          </div>

          {loaded && (
            <div className="player-panels">
              {loaded.gameState.players.map((player, index) => (
                <div className={`player-panel ${index === loaded.gameState.activePlayer ? "active-player" : ""}`} key={`${player.armyType}-${index}`}>
                  <div className="army-line">
                    <span>{player.armyType}</span>
                    <strong>{player.co}</strong>
                  </div>
                  <div className="stat-line">
                    <span>{formatMoney(player.funds)}G</span>
                    <span>{player["power-meter"].charge}/{player["power-meter"]["star-value"] * (player["power-meter"]["cop-stars"] + player["power-meter"]["scop-stars"])}</span>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>

        <aside className="inspector">
          <div className="inspector-block">
            <h2>{loaded?.label ?? "No game"}</h2>
            <dl>
              <div>
                <dt>Game</dt>
                <dd>{loaded?.gameState.gameId ?? "-"}</dd>
              </div>
              <div>
                <dt>Map</dt>
                <dd>
                  {mapCols} x {mapRows}
                </dd>
              </div>
              <div>
                <dt>Source</dt>
                <dd>{loaded?.source ?? "-"}</dd>
              </div>
            </dl>
          </div>

          <div className="inspector-block">
            <h2>{displayCoordinate ? `(${displayCoordinate[0]}, ${displayCoordinate[1]})` : "Tile"}</h2>
            {tile ? (
              <dl>
                <div>
                  <dt>Terrain</dt>
                  <dd>{terrainDisplayName(tile.terrain)}</dd>
                </div>
                <div>
                  <dt>Owner</dt>
                  <dd>{tile.property?.owner ?? "-"}</dd>
                </div>
                <div>
                  <dt>Capture</dt>
                  <dd>{tile.property?.["capture-points"] ?? "-"}</dd>
                </div>
                <div>
                  <dt>Unit</dt>
                  <dd>{tile.unit ? `${tile.unit.owner} ${tile.unit.type}` : "-"}</dd>
                </div>
                {tile.unit && (
                  <>
                    <div>
                      <dt>HP</dt>
                      <dd>{tile.unit.health}</dd>
                    </div>
                    <div>
                      <dt>Fuel / Ammo</dt>
                      <dd>
                        {tile.unit.fuel} / {tile.unit.ammo}
                      </dd>
                    </div>
                    <div>
                      <dt>Cargo</dt>
                      <dd>{tile.unit["loaded-units"]?.length ?? 0}</dd>
                    </div>
                  </>
                )}
              </dl>
            ) : (
              <p className="muted">Select a tile.</p>
            )}
          </div>

          <div className="inspector-block">
            <h2>Actions</h2>
            {buyItems.length > 0 || commandActions.length > 0 ? (
              <div className="action-list">
                {buyItems.length > 0 && activePlayer && (
                  <div className="buy-menu" aria-label="Deployment menu">
                    {buyItems.map((item) => {
                      const assetPath = unitAssetPath(activePlayer.armyType, item.unit, false);
                      return (
                        <button
                          type="button"
                          className="buy-menu-row"
                          key={`${coordinateKey(item.action.source ?? [0, 0])}-${item.unit}`}
                          onClick={() => submitServerAction(item.action)}
                          disabled={!canSubmitDisplayedActions || isLoading}
                          aria-label={`Buy ${item.label} for ${formatMoney(item.cost)}`}
                        >
                          <span className="buy-unit-icon" aria-hidden="true">
                            {assetPath ? <img src={assetPath} alt="" /> : unitFallbackLabel(item.unit)}
                          </span>
                          <span className="buy-unit-name">{item.label}</span>
                          <span className="buy-unit-cost">{formatMoney(item.cost)}</span>
                        </button>
                      );
                    })}
                  </div>
                )}
                {commandActions.map((action, index) => (
                  <div className={canSubmitDisplayedActions ? "action-row" : "action-row action-row-readonly"} key={`${coordinateKey(action.source ?? [0, 0])}-${action.type}-${index}`}>
                    {canSubmitDisplayedActions && (
                      <button type="button" onClick={() => submitServerAction(action)} disabled={isLoading}>
                        {labelForAction(action)}
                      </button>
                    )}
                    <pre>{JSON.stringify(action)}</pre>
                  </div>
                ))}
              </div>
            ) : (
              <p className="muted">No actions available.</p>
            )}
          </div>

          <div className="inspector-block">
            <h2>JSON</h2>
            <textarea value={pasteValue} onChange={(event) => setPasteValue(event.target.value)} spellCheck={false} />
            <button type="button" onClick={loadPastedState}>
              Load JSON
            </button>
          </div>
        </aside>
      </section>
    </main>
  );
}
