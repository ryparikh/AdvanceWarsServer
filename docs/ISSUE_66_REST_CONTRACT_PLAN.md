# Issue #66 REST Contract Plan

This file captures the issue #66 REST contract decisions from the design interview so they can be tracked verbatim.

## Settled Design

- `POST /games` takes setup payload, not full state.
- Create request uses `mapId`; v1 supports `lefty` and `mcts`.
- Map id resolves to curated backend JSON templates, not raw `.txt` terrain.
- `players` is required, exactly two players, v1 armies must match template armies.
- `players[0]` maps to template player slot 0 and `players[1]` maps to template player slot 1.
- No `startingPlayer` override in v1; use the template/default initial active player.
- Create CO ids use lowercase kebab-case, while existing state/action serialization stays as-is for now.
- V1 accepts any CO id the engine can parse; remaining CO mechanic gaps stay tracked in focused follow-up issues.
- Settings may be omitted; server fills strict Standard defaults and rejects unsupported non-Standard settings.
- Resolved settings are serialized inside every returned `GameState`, including create, get, and step responses.
- Optional `seed` is accepted for reproducible combat RNG; when omitted, combat RNG may be nondeterministic.
- `seed` is request-only/debug metadata and is not serialized in normal state responses.
- No reset endpoint; recreate from same setup.
- Server-generated `gameId` only.
- Games are process-local and retained in memory until server restart, including terminal games.
- No delete/list/persistence/TTL behavior in v1.
- No optimistic concurrency/version token or state-version field in v1; action legality is checked against current server state at mutation time.
- New canonical routes:
  - `POST /games`
  - `GET /games/:gameId`
  - `GET /games/:gameId/actions`
  - `GET /games/:gameId/actions?x=4&y=7`
  - `POST /games/:gameId/actions`
- Do not keep legacy `/actions/:gameid` or `/actions/:gameid/:x/:y` routes as supported aliases.
- Successful create/get/step returns full raw `GameState`.
- Legal action list returns an envelope with `gameId`, `activePlayer`, optional `source`, and `actions`:
  ```json
  {
    "gameId": "abc",
    "activePlayer": 0,
    "source": [4, 7],
    "actions": [
      { "type": "move-wait", "source": [1, 2], "target": [1, 3] },
      { "type": "end-turn" }
    ]
  }
  ```
  For all legal actions, omit `source`.
- Action post body is a raw action object.
- Illegal action returns non-2xx error plus current authoritative game state.
- Error responses use status codes plus stable `error.code`.
- REST step validates against generated legal actions before mutation and applies exactly one action.
- Add `terminalReason`, including `heuristic-resign` for current heuristic behavior.
- Fix `Action::operator==` to include `unloadIndex`.
- Unknown ids and unknown JSON fields are parse/validation errors, not silent `Invalid` enums or ignored extras.
- Opened #138 for configurable heuristic resign.
- Opened #139 for full map catalog expansion.
- Opened #140 for explicit or randomized starting player setup.
- Opened #141 for game cache persistence, cleanup, or TTL policy.

## Create Payload Direction

Minimal v1 request:

```json
{
  "mapId": "lefty",
  "players": [
    { "co": "andy", "armyType": "orange-star" },
    { "co": "adder", "armyType": "blue-moon" }
  ],
  "seed": 8675309
}
```

Settings may be omitted. If provided, `mode` belongs inside `settings`, not at the top level. The server resolves strict Standard defaults and serializes the resolved settings in returned game state.

Resolved v1 settings:

```json
{
  "settings": {
    "mode": "standard",
    "fog": false,
    "weather": "clear",
    "coPowers": true,
    "tags": false,
    "startingFunds": 0,
    "incomePerProperty": 1000,
    "unitCap": 50,
    "captureLimit": 21,
    "bannedUnits": ["blackbomb"]
  }
}
```

Do not include day limit, timer/live settings, lab units, High Funds, tag COs, random weather, or fog metadata in v1. Reject those if submitted.

Deferred settings are already tracked by follow-up issues:

- Day-limit terminal rules: #74.
- Lab-unit production requirements: #76.
- Timer/live enforcement and timeout metadata: #82.
- Fog of War, visibility, and fog-only metadata: #90.
- High Funds mode: #91.
- Random weather mode/settings: #92.
- Tag powers and tag-team modes: #96.

## Error Direction

Use HTTP status codes plus a stable JSON error envelope:

```json
{
  "error": {
    "code": "unsupported-setting",
    "message": "Fog of War is not supported for standard games yet.",
    "details": {
      "field": "settings.fog",
      "value": true
    }
  }
}
```

For invalid actions against an existing game, include the current authoritative game state:

```json
{
  "error": {
    "code": "illegal-action",
    "message": "Action is not legal for the current player."
  },
  "game": { "...current unchanged GameState...": true }
}
```

Recommended status mapping:

- `400 Bad Request`: malformed JSON, wrong field type, invalid query shape, unknown action/unit id in an action payload.
- `404 Not Found`: unknown `gameId`.
- `409 Conflict`: action submitted after terminal state.
- `422 Unprocessable Entity`: valid JSON but invalid setup/action, such as unknown `mapId`, unsupported setting, unsupported CO/army, illegal action, or out-of-bounds coordinate.
- `201 Created`: successful `POST /games`.
- `200 OK`: successful get/list/step.

`POST /games` should also set `Location: /games/:gameId` while returning the full `GameState` body.

Canonical REST routes should set `Content-Type: application/json` on JSON responses and support browser CORS preflight:

- Keep `Access-Control-Allow-Origin: *` for v1.
- Support `OPTIONS` for the canonical routes.
- Allow `GET`, `POST`, and `OPTIONS`.
- Allow `Content-Type` so browser clients can submit JSON setup/action payloads.
