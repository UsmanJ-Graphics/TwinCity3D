#!/usr/bin/env python3
"""
Phase 7 — Population exposure data.

Aggregates an estimated resident population per zone from building
footprints already produced by the GIS pipeline (buildings.json + zones.json
in data/processed/), and writes data/processed/population.json for the C++
engine (PopulationLoader, Phase 7) to consume.

Live-first, fallback-second, same pattern as fetch_weather.py and
download_data.py:

  1. Attempt a live WorldPop aggregate-population query for the whole study
     area bounding box (WorldPop's public stats API). This gives a single
     REAL total-population number for the study area, if the API/network is
     reachable.
  2. Always compute a per-building population ESTIMATE from footprint area,
     estimated floor count, and per-building-type occupancy assumptions --
     this is a heuristic occupancy model, not measured data, and is never
     labeled as anything else.
  3. Per-zone population = sum of that zone's building estimates.
     - If step 1 succeeded, the per-zone values are additionally *rescaled*
       so they sum to the real WorldPop total (data_source =
       "worldpop_live_disaggregated": the TOTAL is real, the split across
       zones is still modelled).
     - If step 1 failed (typical in a sandboxed/offline environment — expect
       this to be the common case), the raw per-building estimates are used
       as-is (data_source = "estimated_model": nothing here is a live
       measurement).

Critical Engineering Rule #2: never present modelled numbers as measured
ones. This script keeps the two clearly separated in _meta and in
data_source, exactly as fetch_weather.py does for temperature and
preprocess_osm.py does for building heights.
"""

import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROCESSED_DIR = os.path.join(REPO_ROOT, "data", "processed")
BBOX_PATH = os.path.join(REPO_ROOT, "data", "raw", "study_area_bbox.json")

BUILDINGS_PATH = os.path.join(PROCESSED_DIR, "buildings.json")
ZONES_PATH = os.path.join(PROCESSED_DIR, "zones.json")
OUTPUT_PATH = os.path.join(PROCESSED_DIR, "population.json")

WORLDPOP_URL = "https://api.worldpop.org/v1/services/stats"
WORLDPOP_DATASET = "wpgppop"
WORLDPOP_YEAR = 2020  # most recent widely-available global WorldPop layer
WORLDPOP_TIMEOUT_SEC = 15

# --- Heuristic occupancy model -------------------------------------------
# Persons per square meter of FLOOR area (not footprint) per building type.
# These are rough, documented planning-level assumptions for a dense Lahore
# residential/commercial corridor, not a validated census figure -- see
# docs/data_sources.md. Commercial/retail/unknown types get 0 or near-0:
# this model estimates RESIDENT population, not daytime worker/visitor
# headcount (that distinction matters for Phase 8's exposure scoring).
PERSONS_PER_SQM_FLOOR = {
    "house": 0.05,
    "residential": 0.06,
    "apartments": 0.08,
    "retail": 0.0,
    "commercial": 0.0,
    "unknown": 0.02,  # conservative guess for untagged buildings
}
METERS_PER_FLOOR = 3.0


def load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def polygon_area(poly):
    """Shoelace formula; poly is a list of [x, z] local-meter points."""
    n = len(poly)
    if n < 3:
        return 0.0
    area = 0.0
    for i in range(n):
        x1, z1 = poly[i]
        x2, z2 = poly[(i + 1) % n]
        area += x1 * z2 - x2 * z1
    return abs(area) * 0.5


def estimate_building_population(building):
    footprint = building.get("polygon", [])
    area = polygon_area(footprint)
    height = building.get("height", 6.0) or 6.0
    floors = max(1, round(height / METERS_PER_FLOOR))
    btype = building.get("building_type", "unknown")
    density = PERSONS_PER_SQM_FLOOR.get(btype, PERSONS_PER_SQM_FLOOR["unknown"])
    return area * floors * density


def try_fetch_worldpop_total(bbox):
    """Best-effort live WorldPop total-population query for the study area
    bbox. Returns a float total population, or None on ANY failure (missing
    bbox file, no network, non-200, unexpected shape, timeout, ...) --
    exactly like fetch_weather.py's fetch_live()."""
    try:
        s, w, n, e = bbox["latitude_min"], bbox["longitude_min"], bbox["latitude_max"], bbox["longitude_max"]
        geojson_geom = {
            "type": "Polygon",
            "coordinates": [[[w, s], [e, s], [e, n], [w, n], [w, s]]],
        }
        params = {
            "dataset": WORLDPOP_DATASET,
            "year": str(WORLDPOP_YEAR),
            "geojson": json.dumps(geojson_geom),
            "runasync": "false",
        }
        query = "&".join(f"{k}={urllib.parse.quote(v, safe='')}" for k, v in params.items())
        url = f"{WORLDPOP_URL}?{query}"
        req = urllib.request.Request(url, headers={"User-Agent": "LahoreDigitalTwin/0.1"})
        with urllib.request.urlopen(req, timeout=WORLDPOP_TIMEOUT_SEC) as resp:
            if resp.status != 200:
                raise RuntimeError(f"HTTP {resp.status}")
            payload = json.loads(resp.read().decode("utf-8"))
        total = payload.get("data", {}).get("total_population")
        if total is None:
            raise RuntimeError("response missing data.total_population")
        return float(total)
    except Exception as e:
        print(f"[estimate_population] WorldPop live query failed/unavailable "
              f"({e.__class__.__name__}: {e}); using estimated_model only.", file=sys.stderr)
        return None


def main():
    if not os.path.exists(BUILDINGS_PATH):
        print(f"[estimate_population] ERROR: {BUILDINGS_PATH} not found. Run "
              f"build_sample_data.py or the OSM pipeline first.", file=sys.stderr)
        sys.exit(1)
    if not os.path.exists(ZONES_PATH):
        print(f"[estimate_population] ERROR: {ZONES_PATH} not found. Run "
              f"build_sample_data.py or the OSM pipeline first.", file=sys.stderr)
        sys.exit(1)

    buildings_doc = load_json(BUILDINGS_PATH)
    zones_doc = load_json(ZONES_PATH)

    buildings = buildings_doc.get("buildings", [])
    zones = zones_doc.get("zones", [])
    if not zones:
        print(f"[estimate_population] ERROR: no zones found in {ZONES_PATH}", file=sys.stderr)
        sys.exit(1)

    zone_ids = [z["id"] for z in zones]
    zone_area_m2 = {
        z["id"]: max(0.0, (z["max_x"] - z["min_x"]) * (z["max_z"] - z["min_z"]))
        for z in zones
    }
    heuristic_population = {zid: 0.0 for zid in zone_ids}

    unassigned = 0
    for b in buildings:
        zid = b.get("zone_id", -1)
        if zid not in heuristic_population:
            unassigned += 1
            continue
        heuristic_population[zid] += estimate_building_population(b)

    heuristic_total = sum(heuristic_population.values())

    live_total = None
    if os.path.exists(BBOX_PATH):
        bbox = load_json(BBOX_PATH)
        live_total = try_fetch_worldpop_total(bbox)
    else:
        print(f"[estimate_population] {BBOX_PATH} not found; skipping live WorldPop "
              f"attempt, using estimated_model only.", file=sys.stderr)

    if live_total is not None and heuristic_total > 0:
        scale = live_total / heuristic_total
        data_source = "worldpop_live_disaggregated"
        method = "worldpop_total_rescaled_by_building_floor_area_model"
        print(f"[estimate_population] Live WorldPop total={live_total:.0f}; "
              f"rescaling heuristic distribution (heuristic_total={heuristic_total:.0f}, "
              f"scale={scale:.3f})")
    else:
        scale = 1.0
        data_source = "estimated_model"
        method = "building_floor_area_occupancy_model"

    zones_out = []
    total_population = 0
    for zid in zone_ids:
        pop = int(round(heuristic_population[zid] * scale))
        area = zone_area_m2[zid]
        density_per_km2 = (pop / (area / 1_000_000.0)) if area > 0 else 0.0
        zones_out.append({
            "zone_id": zid,
            "population": pop,
            "population_density_per_km2": round(density_per_km2, 1),
        })
        total_population += pop

    output = {
        "data_source": data_source,
        "method": method,
        "zones": zones_out,
        "_meta": {
            "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "total_population": total_population,
            "unassigned_buildings": unassigned,
            "notes": (
                "Population is a MODELLED ESTIMATE derived from building footprint "
                "area, estimated floor count (height / 3m), and per-building-type "
                "occupancy assumptions -- NOT a census or a direct WorldPop pixel "
                "read. When a live WorldPop total is available, only the study-area "
                "TOTAL is real; the split across zones is still estimated. See "
                "docs/data_sources.md."
            ),
        },
    }

    os.makedirs(PROCESSED_DIR, exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as f:
        json.dump(output, f, indent=2)

    print(f"[estimate_population] wrote {OUTPUT_PATH} "
          f"(data_source={data_source}, total_population={total_population}, "
          f"zones={len(zones_out)}, unassigned_buildings={unassigned})")


if __name__ == "__main__":
    main()
