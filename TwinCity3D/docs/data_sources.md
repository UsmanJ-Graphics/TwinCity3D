# Phase 2 — Data Sources

## OpenStreetMap (buildings, roads, green areas, facilities)

- **Primary path:** `python/download_data.py` queries the live Overpass API
  (`https://overpass-api.de/api/interpreter`), clipped to the Phase 1
  bounding box, and caches the result to `data/raw/osm_raw.json`
  (`_meta.source = "overpass_live"`).
- **Fallback path:** if the live query fails (no network, Overpass down, or
  a sandboxed environment with no egress to overpass-api.de — this is the
  case in the current build/dev environment), the pipeline falls back to
  `data/raw/osm_sample.json`, generated once by `python/build_sample_data.py`.
  This is clearly marked **SAMPLE / DEMO data**, not a live extract:
  - top-level `_meta.source = "sample_fallback"`
  - every element carries `"sample": true`
  - every processed record carries `"sample_data": true`
- `python/preprocess_osm.py` reads whichever raw file exists (live preferred)
  and produces identically-shaped output either way, so nothing downstream
  needs to know or care which source was used — it just needs to check the
  `data_source` field if it wants to display a "SAMPLE DATA" badge in the UI
  (recommended for Phase 9+ so the demo never silently passes off synthetic
  data as real).

## Why sample data exists at all

Per Critical Engineering Rule #2 ("Never fake an API response and present it
as real") and Phase 22 ("Fallback Mode"), the project must keep working
offline and must never disguise synthetic data as a real API response. The
sample dataset is the opposite of disguising: it is loudly labeled at every
level (`_meta`, per-element, per-record) so the C++ engine and the UI can
choose to show a "Demo data" indicator.

The sample dataset's broad character (a dense commercial spine along MM Alam
Road with quieter residential side streets and a few green pockets) is drawn
from the reasoning documented in `phase1_study_area.md`. Individual building
footprints, names, and heights within it are synthetic — they exist to give
the rendering, heat-risk, and scenario phases something structurally
realistic to work with, not to represent surveyed buildings.

## Coordinate system

All raw lat/lon is converted to the engine's local Cartesian system via
`python/coordinate_utils.py` before being written to `data/processed/`:

- `x` = east-west meters, `z` = north-south meters, origin = study-area
  bbox centroid (`31.5180°N, 74.3503°E`)
- Elevation (`y`) is not part of the 2D OSM projection; it's introduced in
  Phase 3 (extrusion) from each building's estimated `height`.
- Equirectangular (flat-earth) approximation — accurate to well under a
  centimeter at this ~1.4 km scale, and far simpler than a projected CRS.

## Zone grid (feeds Phase 4)

`preprocess_osm.py` also assigns every building/road/green area/facility a
`zone_id` from a regular **4×4 grid** (16 zones) over the study area's local
bounds, written to `data/processed/zones.json`. This is an MVP stand-in for
real administrative/block boundaries — swapping it for a real block
shapefile later would only require changing `zone_grid()` in
`preprocess_osm.py`; every downstream file format stays the same.

## Building height estimation

Priority order, each tagged so the UI/README can distinguish real
measurements from guesses:

1. OSM `height` tag → `height_source: "osm_height_tag"`
2. OSM `building:levels` × 3 m/level → `height_source: "estimated_from_levels"`
3. Per-building-type default (commercial 12 m, apartments 9 m, retail 8 m,
   residential 6 m, house 4 m, unknown 6 m) → `height_source: "estimated_default"`

None of these are claimed as surveyed/authoritative heights anywhere in the
pipeline or (in later phases) the UI.

## Population and weather data

Not part of Phase 2. WorldPop population aggregation is Phase 7;
Open-Meteo weather ingestion is Phase 5. Phase 2 only produces the static
spatial layers (buildings, roads, green areas, facilities) and the zone
grid they're assigned to.
