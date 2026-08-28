#!/usr/bin/env python3
"""Build Phase 18's offline environmental-zone layer.

This is deliberately an OSM land-cover proxy, NOT satellite-derived NDVI or
land-surface temperature. A future raster preprocessor can write the same
zone schema with data_source="satellite_processed" without changing C++.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "data" / "processed" / "satellite_environment.json"

def main():
    zones = json.loads((ROOT / "data" / "processed" / "zones.json").read_text(encoding="utf-8"))["zones"]
    buildings = json.loads((ROOT / "data" / "processed" / "buildings.json").read_text(encoding="utf-8"))["buildings"]
    green = json.loads((ROOT / "data" / "processed" / "green_areas.json").read_text(encoding="utf-8"))["green_areas"]

    def area(poly):
        return abs(sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1]
                       for i in range(len(poly)))) * 0.5 if len(poly) >= 3 else 0.0

    built = {z["id"]: 0.0 for z in zones}; vegetation = dict(built)
    for item in buildings: built[item.get("zone_id", -1)] = built.get(item.get("zone_id", -1), 0.0) + area(item.get("polygon", []))
    for item in green: vegetation[item.get("zone_id", -1)] = vegetation.get(item.get("zone_id", -1), 0.0) + area(item.get("polygon", []))

    output = []
    for z in zones:
        b = z["bounds_local"]
        zone_area = max(1.0, (b["x_max"] - b["x_min"]) * (b["z_max"] - b["z_min"]))
        zid = z["id"]
        output.append({"zone_id": zid,
                       "vegetation_index": round(min(1.0, vegetation[zid] / zone_area), 4),
                       "built_up_index": round(min(1.0, built[zid] / zone_area), 4)})

    OUT.write_text(json.dumps({"data_source": "osm_landcover_proxy",
                                "method": "zone_coverage_from_live_osm_geometry",
                                "zones": output,
                                "notes": "Offline environmental proxy only; not satellite imagery, NDVI, or LST."}, indent=2), encoding="utf-8")
    print(f"[build_environmental_layer] wrote {OUT} ({len(output)} zones; OSM proxy)")

if __name__ == "__main__": main()
