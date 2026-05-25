# API Notes

Last reviewed: 2026-05-25

The C++ engine is authoritative for legal actions and state transitions. REST clients should create games, fetch state, request legal actions, and submit exactly one server-validated action at a time.

## Routes

| Method | Route | Purpose |
| --- | --- | --- |
| `POST` | `/games` | Create a Standard game from a setup payload. |
| `GET` | `/games/:gameid` | Fetch the current authoritative game state. |
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

`settings` is optional. If present, `mode` belongs inside `settings`; v1 accepts only strict Standard values. `seed` is optional and request-only: it enables deterministic combat RNG but is not returned in normal API game-state responses.

Supported v1 map ids are `lefty` and `mcts`.

## Responses

Successful create returns `201 Created`, a full game-state JSON body, and `Location: /games/:gameId`. Successful get and step responses return `200 OK` with the full current game state. Game-state responses include resolved `settings` and `terminalReason`; active games use `null` for `terminalReason`.

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
- `422 Unprocessable Entity`: valid JSON but invalid setup/action, such as unknown `mapId`, unsupported setting, unsupported CO/army, illegal action, or invalid coordinate.

## Lifecycle

Games are process-local and retained in memory until server restart, including terminal games. There is no delete, list, reset, persistence, TTL, or optimistic concurrency token in v1.
