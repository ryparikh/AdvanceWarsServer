#!/usr/bin/env python3
"""Generate compact JSON combat matrix fixtures for unit-owned tests.

The generated fixtures intentionally snapshot current engine behavior from the
checked-in unit damage tables and terrain data. Keep this script close to the
JSON tests so future combat-data audits can regenerate or check the matrix
without hand-counting thousands of cases.
"""

from __future__ import annotations

import argparse
import json
import math
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
UNITS_ROOT = REPO_ROOT / "AdvanceWarsServer" / "test" / "json" / "units"


UNIT_ORDER = [
    "anti-air",
    "apc",
    "artillery",
    "bcopter",
    "battleship",
    "blackboat",
    "blackbomb",
    "bomber",
    "carrier",
    "crusier",
    "fighter",
    "infantry",
    "lander",
    "medium-tank",
    "mech",
    "megatank",
    "missile",
    "neotank",
    "piperunner",
    "recon",
    "rocket",
    "stealth",
    "sub",
    "tcopter",
    "tank",
]


@dataclass(frozen=True)
class UnitSpec:
    movement: str
    cost: int
    ammo: int
    fuel: int
    min_range: int
    primary_weapon: str
    secondary_weapon: str


UNITS: dict[str, UnitSpec] = {
    "anti-air": UnitSpec("treads", 8000, 9, 60, 1, "vulcan-cannon", "invalid"),
    "apc": UnitSpec("treads", 5000, 0, 60, -1, "invalid", "invalid"),
    "artillery": UnitSpec("treads", 6000, 9, 50, 2, "cannon", "invalid"),
    "bcopter": UnitSpec("air", 9000, 6, 99, 1, "air-to-surface-missiles", "machine-gun"),
    "battleship": UnitSpec("sea", 28000, 9, 99, 2, "cannon", "invalid"),
    "blackboat": UnitSpec("lander", 7500, 0, 50, -1, "invalid", "invalid"),
    "blackbomb": UnitSpec("air", 25000, 0, 45, -1, "invalid", "invalid"),
    "bomber": UnitSpec("air", 22000, 9, 99, 1, "bombs", "invalid"),
    "carrier": UnitSpec("sea", 30000, 9, 99, 3, "anti-air-missiles", "invalid"),
    "crusier": UnitSpec("sea", 18000, 9, 99, 1, "anti-sub-missiles", "anti-air-gun"),
    "fighter": UnitSpec("air", 20000, 9, 99, 1, "air-to-air-missiles", "invalid"),
    "infantry": UnitSpec("foot", 1000, 0, 99, 1, "machine-gun", "invalid"),
    "lander": UnitSpec("lander", 12000, 0, 99, -1, "invalid", "invalid"),
    "medium-tank": UnitSpec("treads", 16000, 8, 50, 1, "medium-cannon", "machine-gun"),
    "mech": UnitSpec("boots", 3000, 3, 70, 1, "bazooka", "machine-gun"),
    "megatank": UnitSpec("treads", 28000, 3, 50, 1, "mega-cannon", "machine-gun"),
    "missile": UnitSpec("tires", 12000, 6, 50, 3, "anti-air-missiles", "invalid"),
    "neotank": UnitSpec("treads", 22000, 9, 99, 1, "neo-cannon", "machine-gun"),
    "piperunner": UnitSpec("pipe", 20000, 9, 99, 2, "pipe-cannon", "invalid"),
    "recon": UnitSpec("tires", 4000, 0, 80, 1, "machine-gun", "invalid"),
    "rocket": UnitSpec("tires", 15000, 6, 50, 3, "rockets", "invalid"),
    "stealth": UnitSpec("air", 24000, 6, 60, 1, "omni-missile", "invalid"),
    "sub": UnitSpec("sea", 20000, 6, 60, 1, "torpedoes", "invalid"),
    "tcopter": UnitSpec("air", 5000, 0, 99, -1, "invalid", "invalid"),
    "tank": UnitSpec("treads", 7000, 9, 70, 1, "light-cannon", "machine-gun"),
}


PRIMARY_DAMAGE = {
    "anti-air": [45, 50, 50, 120, -1, -1, 120, 75, -1, -1, 65, 105, -1, 10, 105, 1, 55, 5, 25, 60, 55, 75, -1, 120, 25],
    "apc": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "artillery": [75, 70, 75, -1, 40, 55, -1, -1, 45, 65, -1, 90, 55, 45, 85, 15, 80, 40, 70, 80, 80, -1, 60, -1, 70],
    "bcopter": [25, 60, 65, -1, 25, 25, -1, -1, 25, 55, -1, -1, 25, 25, -1, 10, 65, 20, 55, 55, 65, -1, 25, -1, 55],
    "battleship": [85, 80, 80, -1, 50, 95, -1, -1, 60, 95, -1, 95, 95, 55, 90, 25, 90, 50, 80, 90, 85, -1, 95, -1, 80],
    "blackboat": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "blackbomb": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "bomber": [95, 105, 105, -1, 75, 95, -1, -1, 75, 85, -1, 110, 95, 95, 110, 35, 105, 90, 105, 105, 105, -1, 95, -1, 105],
    "carrier": [-1, -1, -1, 115, -1, -1, 120, 100, -1, -1, 100, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 100, -1, 115, -1],
    "crusier": [-1, -1, -1, -1, -1, 25, -1, -1, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 90, -1, -1],
    "fighter": [-1, -1, -1, 100, -1, -1, 120, 100, -1, -1, 55, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 85, -1, 100, -1],
    "infantry": [5, 14, 15, 7, -1, -1, -1, -1, -1, -1, -1, 55, -1, 1, 45, 1, 25, 1, 5, 12, 25, -1, -1, 30, 5],
    "lander": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "medium-tank": [105, 105, 105, -1, 10, 35, -1, -1, 10, 45, -1, -1, 35, 55, -1, 25, 105, 45, 85, 105, 105, -1, 10, -1, 85],
    "mech": [65, 75, 70, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 15, -1, 5, 85, 15, 55, 85, 85, -1, -1, -1, 55],
    "megatank": [195, 195, 195, -1, 45, 105, -1, -1, 45, 65, -1, -1, 75, 125, -1, 65, 195, 115, 180, 195, 195, -1, 45, -1, 180],
    "missile": [-1, -1, -1, 120, -1, -1, 120, 100, -1, -1, 100, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 100, -1, 120, -1],
    "neotank": [115, 125, 115, -1, 15, 40, -1, -1, 15, 50, -1, -1, 40, 75, -1, 35, 125, 55, 105, 125, 125, -1, 15, -1, 105],
    "piperunner": [85, 80, 80, 105, 55, 60, 120, 75, 60, 60, 65, 95, 60, 55, 90, 25, 90, 50, 80, 90, 85, 75, 85, 105, 80],
    "recon": [4, 45, 45, 10, -1, -1, -1, -1, -1, -1, -1, 70, -1, 1, 65, 1, 28, 1, 6, 35, 55, -1, -1, 35, 6],
    "rocket": [85, 80, 80, -1, 55, 60, -1, -1, 60, 85, -1, 95, 60, 55, 90, 25, 90, 50, 80, 90, 85, -1, 85, -1, 80],
    "stealth": [50, 85, 75, 85, 45, 65, 120, 70, 45, 35, 45, 90, 65, 70, 90, 15, 85, 60, 80, 85, 85, 55, 55, 95, 75],
    "sub": [-1, -1, -1, -1, 55, 95, -1, -1, 75, 25, -1, -1, 95, -1, -1, -1, -1, -1, -1, -1, -1, -1, 55, -1, -1],
    "tcopter": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "tank": [65, 75, 70, -1, 1, 10, -1, -1, 1, 5, -1, -1, 10, 15, -1, 10, 85, 15, 55, 85, 85, -1, 1, -1, 55],
}

SECONDARY_DAMAGE = {
    "anti-air": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "apc": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "artillery": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "bcopter": [6, 20, 25, 65, -1, -1, -1, -1, -1, -1, -1, 75, -1, 1, 75, 1, 35, 1, 6, 30, 35, -1, -1, 95, 6],
    "battleship": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "blackboat": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "blackbomb": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "bomber": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "carrier": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "crusier": [-1, -1, -1, 115, -1, -1, 120, 65, -1, -1, 55, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 100, -1, 115, -1],
    "fighter": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "infantry": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "lander": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "medium-tank": [7, 45, 45, 12, -1, -1, -1, -1, -1, -1, -1, 105, -1, 1, 95, 1, 35, 1, 8, 45, 45, -1, -1, 45, 8],
    "mech": [6, 20, 32, 9, -1, -1, -1, -1, -1, -1, -1, 65, -1, 1, 55, 1, 35, 1, 6, 18, 35, -1, -1, 35, 6],
    "megatank": [17, 65, 65, 22, -1, -1, -1, -1, -1, -1, -1, 135, -1, 1, 125, 1, 55, 1, 10, 65, 75, -1, -1, 55, 10],
    "missile": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "neotank": [17, 65, 65, 22, -1, -1, -1, -1, -1, -1, -1, 125, -1, 1, 115, 1, 55, 1, 10, 65, 75, -1, -1, 55, 10],
    "piperunner": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "recon": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "rocket": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "stealth": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "sub": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "tcopter": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
    "tank": [5, 54, 45, 10, -1, -1, -1, -1, -1, -1, -1, 75, -1, 1, 70, 1, 30, 1, 6, 40, 55, -1, -1, 40, 6],
}


@dataclass(frozen=True)
class TerrainSpec:
    file_id: int
    defense: int
    property_owner: str | None = None


TERRAINS: dict[str, TerrainSpec] = {
    "plain": TerrainSpec(1, 1),
    "mountain": TerrainSpec(2, 4),
    "forest": TerrainSpec(3, 2),
    "river": TerrainSpec(4, 0),
    "road": TerrainSpec(15, 0),
    "bridge": TerrainSpec(26, 0),
    "sea": TerrainSpec(28, 0),
    "shoal": TerrainSpec(29, 0),
    "reef": TerrainSpec(33, 1),
    "city": TerrainSpec(34, 3, "neutral"),
    "base": TerrainSpec(35, 3, "neutral"),
    "airport": TerrainSpec(36, 3, "neutral"),
    "port": TerrainSpec(37, 3, "neutral"),
    "headquarters": TerrainSpec(47, 4, "blue-moon"),
    "pipe": TerrainSpec(101, 0),
    "missile-silo": TerrainSpec(112, 3),
    "com-tower": TerrainSpec(133, 3, "neutral"),
    "lab": TerrainSpec(145, 3, "neutral"),
}


TARGET_TERRAINS_BY_MOVEMENT = {
    "air": ["airport", "base", "bridge", "city", "com-tower", "forest", "headquarters", "lab", "missile-silo", "mountain", "plain", "port", "reef", "river", "road", "sea", "shoal"],
    "boots": ["airport", "base", "bridge", "city", "com-tower", "forest", "headquarters", "lab", "missile-silo", "mountain", "plain", "port", "river", "road", "shoal"],
    "foot": ["airport", "base", "bridge", "city", "com-tower", "forest", "headquarters", "lab", "missile-silo", "mountain", "plain", "port", "river", "road", "shoal"],
    "lander": ["port", "reef", "sea", "shoal"],
    "pipe": ["base", "pipe"],
    "sea": ["port", "reef", "sea"],
    "tires": ["airport", "base", "bridge", "city", "com-tower", "forest", "headquarters", "lab", "missile-silo", "plain", "port", "road", "shoal"],
    "treads": ["airport", "base", "bridge", "city", "com-tower", "forest", "headquarters", "lab", "missile-silo", "plain", "port", "road", "shoal"],
}

FULL_MATRIX_UNITS = ["infantry", "mech", "tank", "artillery"]
CHECK_UNITS = ["infantry", "mech", "tank", "medium-tank", "artillery"]
DIRECT_ATTACKERS = {"infantry", "mech", "tank"}
NO_AMMO_LEGAL_ATTACKERS = {"mech", "tank"}
NO_AMMO_INVALID_ATTACKERS = {"mech", "tank", "artillery"}


def damage_row(table: dict[str, list[int]], attacker: str, defender: str) -> int:
    return table[attacker][UNIT_ORDER.index(defender)]


def is_footsoldier(unit: str) -> bool:
    return unit in {"infantry", "mech"}


def is_air(unit: str) -> bool:
    return UNITS[unit].movement == "air"


def base_damage(attacker: str, defender: str, attacker_ammo: int) -> int:
    attacker_spec = UNITS[attacker]
    if is_footsoldier(defender):
        if attacker_spec.primary_weapon == "machine-gun":
            return damage_row(PRIMARY_DAMAGE, attacker, defender)
        if attacker_spec.secondary_weapon == "machine-gun":
            return damage_row(SECONDARY_DAMAGE, attacker, defender)
        if attacker_ammo > 0:
            return damage_row(PRIMARY_DAMAGE, attacker, defender)
        return -1

    if attacker_spec.primary_weapon == "machine-gun":
        return damage_row(PRIMARY_DAMAGE, attacker, defender)
    if attacker_ammo > 0:
        damage = damage_row(PRIMARY_DAMAGE, attacker, defender)
        if damage == -1 and attacker_spec.secondary_weapon != "invalid":
            damage = damage_row(SECONDARY_DAMAGE, attacker, defender)
        return damage
    if attacker_spec.secondary_weapon != "invalid":
        return damage_row(SECONDARY_DAMAGE, attacker, defender)
    return -1


def can_attack(attacker: str, defender: str, attacker_ammo: int) -> bool:
    return base_damage(attacker, defender, attacker_ammo) > -1


def attack_uses_ammo(attacker: str, defender: str, attacker_ammo: int) -> bool:
    return attacker_ammo > 0 and damage_row(PRIMARY_DAMAGE, attacker, defender) > -1


def calculate_damage(
    attacker: str,
    defender: str,
    attacker_health: int,
    defender_health: int,
    defender_terrain: str,
    attacker_ammo: int,
    attacker_luck: str,
) -> int:
    base = base_damage(attacker, defender, attacker_ammo)
    if base == -1:
        return -1

    good_luck = 0 if attacker_luck == "lowest" else 9
    bad_luck = 0
    defender_stars = 0 if is_air(defender) else TERRAINS[defender_terrain].defense
    attacker_visual_health = (attacker_health + 9) // 10
    defender_visual_health = (defender_health + 9) // 10
    damage = (
        (base + good_luck - bad_luck)
        * attacker_visual_health
        / 10.0
        * ((200 - (100 + defender_stars * defender_visual_health)) / 100.0)
    )
    if damage <= 0:
        return 0
    damage = math.ceil(damage * 20.0) / 20.0
    return math.floor(damage)


def unit_json(unit_type: str, owner: str, ammo: int | None = None, health: int = 100, moved: bool = False) -> dict:
    spec = UNITS[unit_type]
    return {
        "ammo": spec.ammo if ammo is None else ammo,
        "fuel": spec.fuel,
        "health": health,
        "hidden": False,
        "moved": moved,
        "owner": owner,
        "type": unit_type,
    }


def tile_json(terrain: str, unit: dict | None = None) -> dict:
    spec = TERRAINS[terrain]
    tile = {"terrain": spec.file_id}
    if spec.property_owner is not None:
        tile["property"] = {
            "capture-points": 20,
            "owner": spec.property_owner,
        }
    if unit is not None:
        tile["unit"] = unit
    return tile


def player_json(army: str, co: str, charge: int, cop_stars: int, scop_stars: int, luck_policy: int) -> dict:
    return {
        "armyType": army,
        "co": co,
        "funds": 0,
        "power-meter": {
            "charge": charge,
            "cop-stars": cop_stars,
            "scop-stars": scop_stars,
            "star-value": 9000,
        },
        "power-status": 0,
        "luck-policy": luck_policy,
    }


def game_state(game_id: str, rows: list[list[dict]], orange_charge: int = 0, blue_charge: int = 0) -> dict:
    return {
        "activePlayer": 0,
        "cap-limit": 21,
        "game-over": False,
        "gameId": game_id,
        "map": rows,
        "players": [
            player_json("orange-star", "Andy", orange_charge, 3, 3, 1),
            player_json("blue-moon", "Adder", blue_charge, 2, 3, 2),
        ],
        "turn-count": 1,
        "unit-cap": 50,
        "winner": -1,
    }


def add_charge(current: int, charge: int, cap: int) -> int:
    return min(current + charge, cap)


def visual_cost_delta(unit_type: str, old_health: int, new_health: int) -> int:
    old_visual = (old_health + 9) // 10
    new_visual = max((new_health + 9) // 10, 0)
    return (old_visual - new_visual) * UNITS[unit_type].cost


def legal_cases(attacker: str, attacker_ammo: int) -> list[tuple[str, str]]:
    cases: list[tuple[str, str]] = []
    for defender in UNIT_ORDER:
        if not can_attack(attacker, defender, attacker_ammo):
            continue
        for terrain in TARGET_TERRAINS_BY_MOVEMENT[UNITS[defender].movement]:
            cases.append((defender, terrain))
    return cases


def invalid_cases(attacker: str, attacker_ammo: int) -> list[tuple[str, str]]:
    cases: list[tuple[str, str]] = []
    for defender in UNIT_ORDER:
        if can_attack(attacker, defender, attacker_ammo):
            continue
        for terrain in TARGET_TERRAINS_BY_MOVEMENT[UNITS[defender].movement]:
            cases.append((defender, terrain))
    return cases


def action_for(attacker: str, row: int) -> dict:
    if attacker in DIRECT_ATTACKERS:
        return {
            "type": "move-attack",
            "source": [0, row],
            "target": [0, row],
            "direction": "east",
        }
    return {
        "type": "attack",
        "source": [0, row],
        "target": [2, row],
    }


def build_legal_fixture(attacker: str, ammo_mode: str) -> dict:
    attacker_ammo = 0 if ammo_mode == "no-ammo" else UNITS[attacker].ammo
    cases = legal_cases(attacker, attacker_ammo)
    rows: list[list[dict]] = []
    final_rows: list[list[dict]] = []
    actions: list[dict] = []
    matrix_cases: list[str] = []
    orange_charge = 0
    blue_charge = 0

    for row_index, (defender, terrain) in enumerate(cases):
        attacker_unit = unit_json(attacker, "orange-star", ammo=attacker_ammo)
        defender_unit = unit_json(defender, "blue-moon")
        if attacker in DIRECT_ATTACKERS:
            row = [
                tile_json("road", attacker_unit),
                tile_json(terrain, defender_unit),
                tile_json("road", unit_json("infantry", "orange-star")),
                tile_json("road", unit_json("infantry", "blue-moon")),
            ]
        else:
            row = [
                tile_json("road", attacker_unit),
                tile_json("road"),
                tile_json(terrain, defender_unit),
                tile_json("road", unit_json("infantry", "orange-star")),
                tile_json("road", unit_json("infantry", "blue-moon")),
            ]
        rows.append(row)
        actions.append(action_for(attacker, row_index))
        matrix_cases.append(f"{attacker}|{ammo_mode}|{defender}|{terrain}")

        final_attacker_health = 100
        final_defender_health = 100
        final_attacker_ammo = attacker_ammo
        final_defender_ammo = UNITS[defender].ammo

        attack_damage = calculate_damage(attacker, defender, 100, 100, terrain, attacker_ammo, "lowest")
        final_defender_health -= attack_damage
        defender_cost_delta = visual_cost_delta(defender, 100, final_defender_health)
        orange_charge = add_charge(orange_charge, defender_cost_delta // 2, 54000)
        blue_charge = add_charge(blue_charge, defender_cost_delta, 45000)
        if attack_uses_ammo(attacker, defender, attacker_ammo):
            final_attacker_ammo -= 1

        defender_survived = final_defender_health > 0
        if defender_survived and UNITS[attacker].min_range == 1 and UNITS[defender].min_range == 1:
            counter_damage = calculate_damage(defender, attacker, final_defender_health, 100, "road", final_defender_ammo, "highest")
            if counter_damage >= 0:
                final_attacker_health -= counter_damage
                attacker_cost_delta = visual_cost_delta(attacker, 100, final_attacker_health)
                blue_charge = add_charge(blue_charge, attacker_cost_delta // 2, 45000)
                orange_charge = add_charge(orange_charge, attacker_cost_delta, 54000)
                if attack_uses_ammo(defender, attacker, final_defender_ammo):
                    final_defender_ammo -= 1

        final_attacker = None
        if final_attacker_health > 0:
            final_attacker = unit_json(attacker, "orange-star", ammo=final_attacker_ammo, health=final_attacker_health, moved=True)

        final_defender = None
        if final_defender_health > 0:
            final_defender = unit_json(defender, "blue-moon", ammo=final_defender_ammo, health=final_defender_health)

        if attacker in DIRECT_ATTACKERS:
            final_row = [
                tile_json("road", final_attacker),
                tile_json(terrain, final_defender),
                tile_json("road", unit_json("infantry", "orange-star")),
                tile_json("road", unit_json("infantry", "blue-moon")),
            ]
        else:
            final_row = [
                tile_json("road", final_attacker),
                tile_json("road"),
                tile_json(terrain, final_defender),
                tile_json("road", unit_json("infantry", "orange-star")),
                tile_json("road", unit_json("infantry", "blue-moon")),
            ]
        final_rows.append(final_row)

    game_id = f"{attacker}-{ammo_mode}-target-terrain-matrix"
    return {
        "initial-game-state": game_state(game_id, rows),
        "actions": actions,
        "final-game-state": game_state(game_id, final_rows, orange_charge, blue_charge),
        "matrix-cases": matrix_cases,
    }


def build_invalid_fixture(attacker: str, ammo_mode: str) -> dict:
    attacker_ammo = 0 if ammo_mode == "no-ammo" else UNITS[attacker].ammo
    cases = invalid_cases(attacker, attacker_ammo)
    rows: list[list[dict]] = []
    failed_actions: list[dict] = []
    matrix_cases: list[str] = []

    for row_index, (defender, terrain) in enumerate(cases):
        attacker_unit = unit_json(attacker, "orange-star", ammo=attacker_ammo)
        defender_unit = unit_json(defender, "blue-moon")
        if attacker in DIRECT_ATTACKERS:
            row = [tile_json("road", attacker_unit), tile_json(terrain, defender_unit)]
        else:
            row = [tile_json("road", attacker_unit), tile_json("road"), tile_json(terrain, defender_unit)]
        rows.append(row)
        failed_actions.append(action_for(attacker, row_index))
        matrix_cases.append(f"{attacker}|{ammo_mode}|invalid|{defender}|{terrain}")

    empty_row = len(rows)
    if attacker in DIRECT_ATTACKERS:
        rows.append([tile_json("road", unit_json(attacker, "orange-star", ammo=attacker_ammo)), tile_json("road")])
    else:
        rows.append([tile_json("road", unit_json(attacker, "orange-star", ammo=attacker_ammo)), tile_json("road"), tile_json("road")])
    failed_actions.append(action_for(attacker, empty_row))
    matrix_cases.append(f"{attacker}|{ammo_mode}|invalid|empty|road")

    friendly_row = len(rows)
    if attacker in DIRECT_ATTACKERS:
        rows.append([
            tile_json("road", unit_json(attacker, "orange-star", ammo=attacker_ammo)),
            tile_json("road", unit_json("infantry", "orange-star")),
        ])
    else:
        rows.append([
            tile_json("road", unit_json(attacker, "orange-star", ammo=attacker_ammo)),
            tile_json("road"),
            tile_json("road", unit_json("infantry", "orange-star")),
        ])
    failed_actions.append(action_for(attacker, friendly_row))
    matrix_cases.append(f"{attacker}|{ammo_mode}|invalid|friendly-infantry|road")

    game_id = f"{attacker}-{ammo_mode}-invalid-target-terrain-matrix"
    return {
        "initial-game-state": game_state(game_id, rows),
        "failedActions": failed_actions,
        "matrix-cases": matrix_cases,
    }


def write_fixture(path: Path, fixture: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(fixture, indent=2) + "\n", encoding="utf-8")


def generate() -> None:
    for attacker in FULL_MATRIX_UNITS:
        matrix_dir = UNITS_ROOT / attacker / "combat" / "matrix"
        write_fixture(matrix_dir / "full-ammo-target-terrain.json", build_legal_fixture(attacker, "full-ammo"))
        write_fixture(matrix_dir / "full-ammo-invalid-target-terrain.json", build_invalid_fixture(attacker, "full-ammo"))
        if attacker in NO_AMMO_LEGAL_ATTACKERS:
            write_fixture(matrix_dir / "no-ammo-target-terrain.json", build_legal_fixture(attacker, "no-ammo"))
        if attacker in NO_AMMO_INVALID_ATTACKERS:
            write_fixture(matrix_dir / "no-ammo-invalid-target-terrain.json", build_invalid_fixture(attacker, "no-ammo"))


def matrix_case_set(path: Path) -> set[str]:
    with path.open(encoding="utf-8") as f:
        return set(json.load(f).get("matrix-cases", []))


def expected_legal_case_set(attacker: str, ammo_mode: str) -> set[str]:
    ammo = 0 if ammo_mode == "no-ammo" else UNITS[attacker].ammo
    return {f"{attacker}|{ammo_mode}|{defender}|{terrain}" for defender, terrain in legal_cases(attacker, ammo)}


def expected_invalid_case_set(attacker: str, ammo_mode: str) -> set[str]:
    ammo = 0 if ammo_mode == "no-ammo" else UNITS[attacker].ammo
    expected = {f"{attacker}|{ammo_mode}|invalid|{defender}|{terrain}" for defender, terrain in invalid_cases(attacker, ammo)}
    expected.add(f"{attacker}|{ammo_mode}|invalid|empty|road")
    expected.add(f"{attacker}|{ammo_mode}|invalid|friendly-infantry|road")
    return expected


def check_compact_matrix(attacker: str, ammo_mode: str, invalid: bool) -> tuple[bool, str]:
    name = f"{ammo_mode}-{'invalid-' if invalid else ''}target-terrain.json"
    path = UNITS_ROOT / attacker / "combat" / "matrix" / name
    if not path.exists():
        return False, f"{attacker} {name}: missing"
    expected = expected_invalid_case_set(attacker, ammo_mode) if invalid else expected_legal_case_set(attacker, ammo_mode)
    actual = matrix_case_set(path)
    if actual != expected:
        return False, f"{attacker} {name}: expected {len(expected)} cases, found {len(actual)}"
    return True, f"{attacker} {name}: {len(actual)} cases"


def count_existing_files(path: Path) -> int:
    if not path.exists():
        return 0
    return sum(1 for item in path.rglob("*.json") if item.is_file())


def check_medium_tank() -> list[tuple[bool, str]]:
    base = UNITS_ROOT / "medium-tank" / "combat"
    checks = [
        ("full-ammo legal target terrain", base / "legal", len(legal_cases("medium-tank", UNITS["medium-tank"].ammo))),
        ("full-ammo invalid target terrain", base / "invalid-target", len(invalid_cases("medium-tank", UNITS["medium-tank"].ammo)) + 2),
        ("no-ammo legal target terrain", base / "no-ammo" / "legal", len(legal_cases("medium-tank", 0))),
        ("no-ammo invalid target terrain", base / "no-ammo" / "invalid-target", len(invalid_cases("medium-tank", 0)) + 2),
    ]
    results = []
    for label, path, expected in checks:
        actual = count_existing_files(path)
        results.append((actual == expected, f"medium-tank {label}: {actual}/{expected} cases"))
    return results


def check() -> int:
    results: list[tuple[bool, str]] = []
    for attacker in CHECK_UNITS:
        if attacker == "medium-tank":
            results.extend(check_medium_tank())
            continue
        results.append(check_compact_matrix(attacker, "full-ammo", invalid=False))
        results.append(check_compact_matrix(attacker, "full-ammo", invalid=True))
        if attacker in NO_AMMO_LEGAL_ATTACKERS:
            results.append(check_compact_matrix(attacker, "no-ammo", invalid=False))
        if attacker in NO_AMMO_INVALID_ATTACKERS:
            results.append(check_compact_matrix(attacker, "no-ammo", invalid=True))

    for ok, message in results:
        prefix = "OK" if ok else "FAIL"
        print(f"{prefix}: {message}")
    return 0 if all(ok for ok, _ in results) else 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="verify generated matrix-case metadata and medium-tank file counts")
    args = parser.parse_args()
    if args.check:
        return check()
    generate()
    return check()


if __name__ == "__main__":
    raise SystemExit(main())
