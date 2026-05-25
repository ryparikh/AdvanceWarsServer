# API Notes

Last reviewed: 2026-05-25

The C++ engine is authoritative for legal actions and state transitions. REST clients should create games, fetch state, request legal actions, and submit exactly one server-validated action at a time.

## Routes

| Method | Route | Purpose |
| --- | --- | --- |
| `POST` | `/games` | Create a Standard game from a setup payload. |
| `GET` | `/games/:gameid` | Fetch the current authoritative game state. |
| `GET` | `/games/:gameid?player=0` | Fetch game state for one player perspective. |
| `GET` | `/games/:gameid/actions` | Fetch all legal actions for the active player. |
| `GET` | `/games/:gameid/actions?x=4&y=7` | Fetch legal actions for one selected coordinate. |
| `POST` | `/games/:gameid/actions` | Submit exactly one action. |
| `OPTIONS` | canonical routes above | Browser CORS preflight. |

Legacy `/actions/:gameid` and `/actions/:gameid/:x/:y` routes are not part of the supported contract.

## Create Game

Minimal request:

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

`settings` is optional. If present, `mode` belongs inside `settings`; v1 accepts Standard-only gameplay settings. Fog, non-clear weather, CO powers off, tags, High Funds income, and non-Standard unit bans are rejected. `unitCap`, `captureLimit`, and `dayLimit` may be configured for Standard setup. `dayLimit` may be `null` or a positive integer; when reached, the highest property count wins and a tie is a draw. `settings.heuristicAutoResign` is a training/early-stop option that defaults to `false`; when set to `true`, the existing army-value heuristic can end a game with `terminalReason: "heuristic-resign"` after a legal step. `seed` is optional and request-only: it enables deterministic combat RNG but is not returned in normal API game-state responses.

Resolved default settings:

```json
{
  "mode": "standard",
  "fog": false,
  "weather": "clear",
  "coPowers": true,
  "tags": false,
  "startingFunds": 0,
  "incomePerProperty": 1000,
  "unitCap": 50,
  "captureLimit": 21,
  "dayLimit": null,
  "bannedUnits": ["blackbomb"],
  "heuristicAutoResign": false
}
```

Supported v1 map ids are `lefty` and `mcts`.

## Responses

Successful create returns `201 Created`, a full game-state JSON body, and `Location: /games/:gameId`. Successful get and step responses return `200 OK` with the full current game state. A step commits exactly the submitted action: if only `end-turn` remains legal after a unit action, the server reports that legal action and waits for the client to submit it. Game-state responses include resolved `settings` and `terminalReason`; active games use `null` for `terminalReason`.

`GET /games/:gameid` without a query is the full authoritative/debug view and includes exact unit `health`. `GET /games/:gameid?player=0` or `player=1` returns a player-perspective view. In a perspective view, enemy Sonja units hide exact HP as `"health": null` with `"hidden-health": true`; the authoritative engine state and Sonja player's own view keep exact HP.

Each player object includes a `power-meter` object. `cop-stars` and `scop-stars` are the CO's meter split, `charge` is current meter charge, and `star-value` is the current value of one star. `cop-threshold` and `scop-threshold` are the current charge thresholds for legal COP and SCOP actions, while `can-use-cop` and `can-use-scop` reflect whether those global power actions are currently available from meter charge.

Legal-action responses use an envelope:

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

For all legal actions, `source` is omitted.

## Errors

Errors use HTTP status codes plus a stable JSON error code:

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

Invalid actions against an existing game include the current authoritative game state:

```json
{
  "error": {
    "code": "illegal-action",
    "message": "Action is not legal for the current player."
  },
  "game": { "...current unchanged GameState...": true }
}
```

Status mapping:

- `400 Bad Request`: malformed JSON, wrong field type, invalid query shape, unknown JSON field, unknown action/unit id.
- `404 Not Found`: unknown `gameId`.
- `409 Conflict`: action submitted after terminal state.
- `422 Unprocessable Entity`: valid JSON but invalid setup/action, such as unknown `mapId`, unsupported setting, unsupported CO/army, illegal action, invalid coordinate, or invalid player perspective.

## Lifecycle

Games are process-local and retained in memory until server restart, including terminal games. There is no delete, list, reset, persistence, TTL, or optimistic concurrency token in v1.
