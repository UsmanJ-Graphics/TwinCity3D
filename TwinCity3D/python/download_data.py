"""
download_data.py

Phase 2 step 1: fetch raw OpenStreetMap data for the Phase 1 study area
(Gulberg III / MM Alam Road corridor, Lahore) and cache it locally.

Behavior:
  1. Try a live Overpass API query clipped to the study-area bounding box.
  2. On success, cache the raw response to data/raw/osm_raw.json and tag it
     source = "overpass_live".
  3. If the request fails (no network, API down, timeout, sandboxed
     environment, etc.), fall back to data/raw/osm_sample.json -- a small,
     hand-built, CLEARLY LABELED sample dataset that mimics real OSM output
     for this corridor. This is a fallback, not a fabricated live response
     (see Phase 22 "Fallback Mode" and Critical Engineering Rule #2: never
     present simulated data as real).

The C++ engine and preprocess_osm.py never call Overpass directly -- they
only ever read data/raw/*.json, so the rest of the pipeline is identical
regardless of which source was used.
"""

import json
import ssl
import sys
import urllib.request
import urllib.error
from pathlib import Path

import certifi

REPO_ROOT = Path(__file__).resolve().parent.parent
BBOX_PATH = REPO_ROOT / "data" / "raw" / "study_area_bbox.json"
RAW_LIVE_PATH = REPO_ROOT / "data" / "raw" / "osm_raw.json"
SAMPLE_PATH = REPO_ROOT / "data" / "raw" / "osm_sample.json"

OVERPASS_URL = "https://overpass-api.de/api/interpreter"
OVERPASS_TIMEOUT_SEC = 30


def build_overpass_query(bbox: dict) -> str:
    s, w, n, e = bbox["latitude_min"], bbox["longitude_min"], bbox["latitude_max"], bbox["longitude_max"]
    bbox_str = f"{s},{w},{n},{e}"
    # way/relation buildings, highways, leisure/landuse green areas, and
    # a handful of facility amenities, all clipped to the bbox.
    return f"""
    [out:json][timeout:{OVERPASS_TIMEOUT_SEC}];
    (
      way["building"]({bbox_str});
      way["highway"]({bbox_str});
      way["leisure"~"park|garden"]({bbox_str});
      way["landuse"~"grass|forest|recreation_ground"]({bbox_str});
      way["natural"="wood"]({bbox_str});
      node["amenity"~"school|hospital|clinic|bank|marketplace"]({bbox_str});
      way["amenity"~"school|hospital|clinic|bank|marketplace"]({bbox_str});
    );
    out geom;
    """


def fetch_live(bbox: dict) -> dict | None:
    query = build_overpass_query(bbox)
    data = f"data={urllib.parse.quote(query)}".encode("utf-8")
    req = urllib.request.Request(
        OVERPASS_URL,
        data=data,
        method="POST",
        headers={
            "Content-Type": "application/x-www-form-urlencoded",
            "Accept": "application/json",
            "User-Agent": "LahoreUrbanHeatDigitalTwin/1.0 (hackathon prototype; contact: your-email@example.com)",
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=OVERPASS_TIMEOUT_SEC) as resp:
            raw = json.loads(resp.read().decode("utf-8"))
            raw["_meta"] = {"source": "overpass_live", "query": query.strip()}
            return raw
    except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError, OSError) as e:
        print(f"[download_data] Overpass fetch failed ({e}); will use fallback sample data.",
              file=sys.stderr)
        return None


def main():
    with open(BBOX_PATH, "r", encoding="utf-8") as f:
        bbox = json.load(f)

    print(f"[download_data] Study area: {bbox['name']}")
    print(f"[download_data] Attempting live Overpass query...")

    live = fetch_live(bbox)

    if live is not None:
        RAW_LIVE_PATH.parent.mkdir(parents=True, exist_ok=True)
        with open(RAW_LIVE_PATH, "w", encoding="utf-8") as f:
            json.dump(live, f, indent=2)
        print(f"[download_data] SUCCESS (live). Cached to {RAW_LIVE_PATH}")
        print("[download_data] Downstream: preprocess_osm.py will read this file.")
        return

    if not SAMPLE_PATH.exists():
        print(f"[download_data] ERROR: live fetch failed and no fallback sample exists "
              f"at {SAMPLE_PATH}. Run build_sample_data.py first, or provide network access "
              f"to overpass-api.de.", file=sys.stderr)
        sys.exit(1)

    print(f"[download_data] FALLBACK MODE: using pre-built sample dataset at {SAMPLE_PATH}")
    print("[download_data] This is SAMPLE/DEMO data, not a live OSM response. "
          "See docs/data_sources.md.")


if __name__ == "__main__":
    main()
