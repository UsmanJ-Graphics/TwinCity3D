"""
Phase 2/3 — Sample data generator.

Produces the same processed JSON shape that preprocess_osm.py would produce
from a live Overpass extract, but populated with clearly-labeled synthetic
geometry (see docs/data_sources.md: "sample_fallback" path).

This exists so the C++ engine (GISLoader / CityMeshBuilder, Phase 3) has
something structurally realistic to render even with no network access to
overpass-api.de from this environment.

Local coordinate system (matches coordinate_utils.py convention):
  x = east-west meters, z = north-south meters, origin = study-area bbox
  centroid (31.5180N, 74.3503E). y (elevation) is added later by the C++
  extrusion step from each building's `height`.

Study area: ~1.4km x 1.4km square (half-extent 700m), a stand-in for the
MM Alam Road corridor described in phase1_study_area.md — a dense
commercial spine along the east-west axis with quieter residential side
streets and a few green pockets.
"""

import json
import os

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "data", "processed")
HALF_EXTENT = 700.0  # meters
DATA_SOURCE = "sample_fallback"

os.makedirs(OUT_DIR, exist_ok=True)


def zone_id_for(x, z, half_extent=HALF_EXTENT, divisions=4):
    """4x4 regular grid over [-half_extent, half_extent]^2, matches
    preprocess_osm.py's zone_grid()."""
    cell = (half_extent * 2.0) / divisions
    col = int((x + half_extent) // cell)
    row = int((z + half_extent) // cell)
    col = max(0, min(divisions - 1, col))
    row = max(0, min(divisions - 1, row))
    return row * divisions + col


def rect_polygon(cx, cz, w, d, rotation_deg=0.0):
    import math
    hw, hd = w / 2.0, d / 2.0
    pts = [(-hw, -hd), (hw, -hd), (hw, hd), (-hw, hd)]
    a = math.radians(rotation_deg)
    ca, sa = math.cos(a), math.sin(a)
    return [[cx + px * ca - pz * sa, cz + px * sa + pz * ca] for px, pz in pts]


def centroid(polygon):
    xs = [p[0] for p in polygon]
    zs = [p[1] for p in polygon]
    return [sum(xs) / len(xs), sum(zs) / len(zs)]


# ---------------------------------------------------------------------------
# Buildings: dense commercial spine along z in [-45, 45], x in [-650, 650];
# quieter residential blocks further out in z, on both sides.
# ---------------------------------------------------------------------------
buildings = []
bid = 1


def add_building(cx, cz, w, d, height, height_source, btype, rotation=0.0):
    global bid
    poly = rect_polygon(cx, cz, w, d, rotation)
    c = centroid(poly)
    buildings.append({
        "id": f"b{bid:03d}",
        "polygon": poly,
        "height": height,
        "height_source": height_source,
        "centroid": c,
        "zone_id": zone_id_for(c[0], c[1]),
        "building_type": btype,
        "sample_data": True,
    })
    bid += 1


# Commercial spine (both sides of MM Alam Road), alternating osm-tagged vs estimated heights
commercial_xs = list(range(-620, 640, 60))
for i, cx in enumerate(commercial_xs):
    h = 10.0 + (i % 4) * 2.0
    src = "osm_height_tag" if i % 3 == 0 else "estimated_from_levels"
    add_building(cx, -28, 34, 20, h, src, "commercial")
    add_building(cx + 15, 30, 30, 22, h - 1.5, "estimated_from_levels", "retail")

# One irregular (L-shaped) commercial block to exercise non-convex triangulation
l_shape = [[-360, -70], [-300, -70], [-300, -95], [-260, -95],
           [-260, -50], [-360, -50]]
c = centroid(l_shape)
buildings.append({
    "id": f"b{bid:03d}", "polygon": l_shape, "height": 14.0,
    "height_source": "osm_height_tag", "centroid": c,
    "zone_id": zone_id_for(c[0], c[1]), "building_type": "commercial",
    "sample_data": True,
})
bid += 1

# Residential side-street blocks, two bands north and south of the spine
for band_z in (170, 260, 350, 440, 520, 600, -170, -260, -350, -440, -520, -600):
    row_xs = range(-580, 600, 90)
    for cx in row_xs:
        is_house = abs(band_z) > 400
        h = 4.0 if is_house else 9.0
        btype = "house" if is_house else "apartments"
        src = "estimated_default"
        add_building(cx, band_z, 16 if is_house else 22, 14 if is_house else 18, h, src, btype)

# ---------------------------------------------------------------------------
# Roads: primary spine + cross streets forming a simple grid
# ---------------------------------------------------------------------------
roads = []
rid = 1


def add_road(polyline, width, road_type):
    global rid
    roads.append({
        "id": f"r{rid:03d}",
        "polyline": [[float(x), float(z)] for x, z in polyline],
        "width": width,
        "road_type": road_type,
        "sample_data": True,
    })
    rid += 1


add_road([(-680, 0), (680, 0)], 14.0, "primary")          # MM Alam Road spine
for cx in range(-600, 660, 200):
    add_road([(cx, -680), (cx, 680)], 9.0, "secondary")    # cross streets
for bz in (170, 350, 520, -170, -350, -520):
    add_road([(-680, bz), (680, bz)], 7.0, "residential")  # side streets

# ---------------------------------------------------------------------------
# Green areas: a central park pocket + two smaller neighbourhood pockets
# ---------------------------------------------------------------------------
green_areas = []
gid = 1


def add_green(polygon, gtype):
    global gid
    c = centroid(polygon)
    green_areas.append({
        "id": f"g{gid:03d}",
        "polygon": polygon,
        "green_type": gtype,
        "centroid": c,
        "zone_id": zone_id_for(c[0], c[1]),
        "sample_data": True,
    })
    gid += 1


add_green(rect_polygon(-420, 400, 90, 70), "park")
add_green(rect_polygon(300, -480, 70, 60), "park")
add_green(rect_polygon(80, 130, 40, 40), "trees")

# ---------------------------------------------------------------------------
# Facilities: one school, one hospital (simple point markers)
# ---------------------------------------------------------------------------
facilities = []
fid = 1


def add_facility(name, ftype, x, z):
    global fid
    facilities.append({
        "id": f"f{fid:03d}",
        "name": name,
        "facility_type": ftype,
        "position": [float(x), float(z)],
        "zone_id": zone_id_for(x, z),
        "sample_data": True,
    })
    fid += 1


add_facility("Sample Community School", "school", -300, 300)
add_facility("Sample District Hospital", "hospital", 350, -300)

# ---------------------------------------------------------------------------
# Zone grid (4x4, matches preprocess_osm.py::zone_grid)
# ---------------------------------------------------------------------------
divisions = 4
cell = (HALF_EXTENT * 2.0) / divisions
zones = []
for row in range(divisions):
    for col in range(divisions):
        zid = row * divisions + col
        zones.append({
            "id": zid,
            "min_x": -HALF_EXTENT + col * cell,
            "max_x": -HALF_EXTENT + (col + 1) * cell,
            "min_z": -HALF_EXTENT + row * cell,
            "max_z": -HALF_EXTENT + (row + 1) * cell,
        })

zones_doc = {
    "data_source": DATA_SOURCE,
    "grid": f"{divisions}x{divisions}",
    "bounds": {"min_x": -HALF_EXTENT, "max_x": HALF_EXTENT,
               "min_z": -HALF_EXTENT, "max_z": HALF_EXTENT},
    "zones": zones,
}

docs = {
    "buildings.json": {"data_source": DATA_SOURCE, "buildings": buildings},
    "roads.json": {"data_source": DATA_SOURCE, "roads": roads},
    "green_areas.json": {"data_source": DATA_SOURCE, "green_areas": green_areas},
    "facilities.json": {"data_source": DATA_SOURCE, "facilities": facilities},
    "zones.json": zones_doc,
}

for filename, payload in docs.items():
    path = os.path.join(OUT_DIR, filename)
    with open(path, "w") as f:
        json.dump(payload, f, indent=2)
    print(f"wrote {path}")

print(f"buildings={len(buildings)} roads={len(roads)} "
      f"green_areas={len(green_areas)} facilities={len(facilities)} zones={len(zones)}")
