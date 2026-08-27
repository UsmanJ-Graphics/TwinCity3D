"""
preprocess_osm.py

Phase 2 step 2: turn raw OSM data (data/raw/osm_raw.json if a live Overpass
fetch succeeded, otherwise data/raw/osm_sample.json) into the simplified,
locally-projected datasets the C++ engine will consume in Phase 3+:

    data/processed/buildings.json
    data/processed/roads.json
    data/processed/green_areas.json
    data/processed/facilities.json
    data/processed/zones.json          (zone grid definition, feeds Phase 4)

Every element is:
  - clipped to the Phase 1 study-area bounding box
  - reprojected from lat/lon into local (x, z) meters via coordinate_utils
  - tagged with a zone_id from a simple regular grid over the study area
    (Phase 4's Zone data model consumes zone_id; the grid itself is
    replaceable later with real administrative/block boundaries without
    changing this script's output shape)

Building heights: OSM building:levels * 3m when present (labeled
"estimated_from_levels"); otherwise a per-type default (labeled
"estimated_default"). No fabricated precision is claimed either way.
"""

import json
from pathlib import Path

from coordinate_utils import load_study_area, project_ring, polygon_centroid_xz

REPO_ROOT = Path(__file__).resolve().parent.parent
RAW_DIR = REPO_ROOT / "data" / "raw"
PROCESSED_DIR = REPO_ROOT / "data" / "processed"

LIVE_PATH = RAW_DIR / "osm_raw.json"
SAMPLE_PATH = RAW_DIR / "osm_sample.json"

# Regular zone grid resolution over the study area (Phase 4 consumes zone_id).
ZONE_GRID_COLS = 4
ZONE_GRID_ROWS = 4

METERS_PER_LEVEL = 3.0
DEFAULT_HEIGHT_BY_TYPE = {
    "house": 4.0,
    "residential": 6.0,
    "apartments": 9.0,
    "retail": 8.0,
    "commercial": 12.0,
    "yes": 6.0,  # generic/unknown building tag
}
FALLBACK_DEFAULT_HEIGHT = 6.0


def load_raw() -> dict:
    if LIVE_PATH.exists():
        print(f"[preprocess_osm] Using LIVE data: {LIVE_PATH}")
        path = LIVE_PATH
    elif SAMPLE_PATH.exists():
        print(f"[preprocess_osm] Using FALLBACK SAMPLE data: {SAMPLE_PATH}")
        path = SAMPLE_PATH
    else:
        raise FileNotFoundError(
            "No raw OSM data found. Run download_data.py (and build_sample_data.py "
            "if offline) first."
        )
    with open(path, "r", encoding="utf-8") as f:
        raw = json.load(f)
    source = raw.get("_meta", {}).get("source", "unknown")
    print(f"[preprocess_osm] Data source tag: {source}")
    return raw


def element_ring_lonlat(el: dict) -> list[list[float]] | None:
    geom = el.get("geometry")
    if not geom:
        return None
    return [[pt["lon"], pt["lat"]] for pt in geom]


def zone_grid(coord_sys):
    """Returns (grid_meta, zone_lookup_fn) for a regular cols x rows grid
    over the study area's local bounds."""
    b = coord_sys.bounds_local()
    x_min, x_max = b["x_min"], b["x_max"]
    z_min, z_max = b["z_min"], b["z_max"]
    cell_w = (x_max - x_min) / ZONE_GRID_COLS
    cell_h = (z_max - z_min) / ZONE_GRID_ROWS

    def zone_id_for(x: float, z: float) -> int:
        col = min(int((x - x_min) / cell_w), ZONE_GRID_COLS - 1) if cell_w > 0 else 0
        row = min(int((z - z_min) / cell_h), ZONE_GRID_ROWS - 1) if cell_h > 0 else 0
        col = max(col, 0)
        row = max(row, 0)
        return row * ZONE_GRID_COLS + col

    zones_meta = []
    for row in range(ZONE_GRID_ROWS):
        for col in range(ZONE_GRID_COLS):
            zid = row * ZONE_GRID_COLS + col
            zones_meta.append({
                "id": zid,
                "grid_row": row,
                "grid_col": col,
                "bounds_local": {
                    "x_min": x_min + col * cell_w,
                    "x_max": x_min + (col + 1) * cell_w,
                    "z_min": z_min + row * cell_h,
                    "z_max": z_min + (row + 1) * cell_h,
                },
            })

    return zones_meta, zone_id_for


def estimate_height(tags: dict) -> tuple[float, str]:
    if "height" in tags:
        try:
            return float(tags["height"]), "osm_height_tag"
        except ValueError:
            pass
    if "building:levels" in tags:
        try:
            levels = float(tags["building:levels"])
            return levels * METERS_PER_LEVEL, "estimated_from_levels"
        except ValueError:
            pass
    btype = tags.get("building", "yes")
    return DEFAULT_HEIGHT_BY_TYPE.get(btype, FALLBACK_DEFAULT_HEIGHT), "estimated_default"


def process_buildings(elements, coord_sys, zone_id_for):
    buildings = []
    for el in elements:
        tags = el.get("tags", {})
        if "building" not in tags or el.get("type") != "way":
            continue
        ring = element_ring_lonlat(el)
        if not ring or len(ring) < 4:
            continue
        polygon_xz = project_ring(coord_sys, ring)
        cx, cz = polygon_centroid_xz(polygon_xz)
        height, height_source = estimate_height(tags)
        buildings.append({
            "id": f"bldg_{el['id']}",
            "osm_id": el["id"],
            "name": tags.get("name"),
            "building_type": tags.get("building", "yes"),
            "polygon": polygon_xz,          # [[x, z], ...] local meters
            "centroid": [cx, cz],
            "height": round(height, 2),
            "height_source": height_source,  # osm_height_tag | estimated_from_levels | estimated_default
            "zone_id": zone_id_for(cx, cz),
            "sample_data": bool(el.get("sample", False)),
        })
    return buildings


def process_roads(elements, coord_sys, zone_id_for):
    roads = []
    for el in elements:
        tags = el.get("tags", {})
        if "highway" not in tags or el.get("type") != "way":
            continue
        geom = el.get("geometry")
        if not geom or len(geom) < 2:
            continue
        polyline_xz = [list(coord_sys.to_local(pt["lat"], pt["lon"])) for pt in geom]
        cx, cz = polygon_centroid_xz(polyline_xz)
        roads.append({
            "id": f"road_{el['id']}",
            "osm_id": el["id"],
            "name": tags.get("name"),
            "road_type": tags["highway"],
            "polyline": polyline_xz,       # [[x, z], ...] local meters
            "zone_id": zone_id_for(cx, cz),
            "sample_data": bool(el.get("sample", False)),
        })
    return roads


def process_green_areas(elements, coord_sys, zone_id_for):
    green_areas = []
    for el in elements:
        tags = el.get("tags", {})
        is_green = (
            tags.get("leisure") in ("park", "garden")
            or tags.get("landuse") in ("grass", "forest", "recreation_ground")
            or tags.get("natural") == "wood"
        )
        if not is_green or el.get("type") != "way":
            continue
        ring = element_ring_lonlat(el)
        if not ring or len(ring) < 4:
            continue
        polygon_xz = project_ring(coord_sys, ring)
        cx, cz = polygon_centroid_xz(polygon_xz)
        green_type = tags.get("leisure") or tags.get("landuse") or tags.get("natural")
        green_areas.append({
            "id": f"green_{el['id']}",
            "osm_id": el["id"],
            "name": tags.get("name"),
            "green_type": green_type,
            "polygon": polygon_xz,
            "centroid": [cx, cz],
            "zone_id": zone_id_for(cx, cz),
            "sample_data": bool(el.get("sample", False)),
        })
    return green_areas


FACILITY_AMENITIES = {"school", "hospital", "clinic", "bank", "marketplace"}


def process_facilities(elements, coord_sys, zone_id_for):
    facilities = []
    for el in elements:
        tags = el.get("tags", {})
        amenity = tags.get("amenity")
        if amenity not in FACILITY_AMENITIES:
            continue

        if el.get("type") == "node":
            lat, lon = el.get("lat"), el.get("lon")
            if lat is None or lon is None:
                continue
            x, z = coord_sys.to_local(lat, lon)
        elif el.get("type") == "way":
            ring = element_ring_lonlat(el)
            if not ring:
                continue
            polygon_xz = project_ring(coord_sys, ring)
            x, z = polygon_centroid_xz(polygon_xz)
        else:
            continue

        facilities.append({
            "id": f"fac_{el['id']}",
            "osm_id": el["id"],
            "name": tags.get("name"),
            "facility_type": amenity,
            "position": [x, z],
            "zone_id": zone_id_for(x, z),
            "sample_data": bool(el.get("sample", False)),
        })
    return facilities


def main():
    coord_sys = load_study_area()
    raw = load_raw()
    elements = raw.get("elements", [])
    data_source = raw.get("_meta", {}).get("source", "unknown")

    zones_meta, zone_id_for = zone_grid(coord_sys)

    buildings = process_buildings(elements, coord_sys, zone_id_for)
    roads = process_roads(elements, coord_sys, zone_id_for)
    green_areas = process_green_areas(elements, coord_sys, zone_id_for)
    facilities = process_facilities(elements, coord_sys, zone_id_for)

    PROCESSED_DIR.mkdir(parents=True, exist_ok=True)

    def write(name, payload):
        out = {
            "data_source": data_source,   # "overpass_live" | "sample_fallback"
            "study_area": raw.get("_meta", {}).get("note", None) if data_source == "sample_fallback" else "Gulberg III / MM Alam Road corridor, Lahore",
            "count": len(payload),
            "items": payload,
        }
        path = PROCESSED_DIR / name
        with open(path, "w", encoding="utf-8") as f:
            json.dump(out, f, indent=2)
        print(f"[preprocess_osm] Wrote {path} ({len(payload)} items)")

    write("buildings.json", buildings)
    write("roads.json", roads)
    write("green_areas.json", green_areas)
    write("facilities.json", facilities)

    zones_path = PROCESSED_DIR / "zones.json"
    with open(zones_path, "w", encoding="utf-8") as f:
        json.dump({
            "grid_cols": ZONE_GRID_COLS,
            "grid_rows": ZONE_GRID_ROWS,
            "zones": zones_meta,
        }, f, indent=2)
    print(f"[preprocess_osm] Wrote {zones_path} ({len(zones_meta)} zones)")

    if data_source == "sample_fallback":
        print("\n[preprocess_osm] NOTE: processed output is derived from SAMPLE/FALLBACK "
              "OSM data (not a live extract). Every item carries \"sample_data\": true. "
              "Re-run download_data.py with network access to overpass-api.de to replace it.")


if __name__ == "__main__":
    main()
