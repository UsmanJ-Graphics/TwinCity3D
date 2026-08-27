"""
coordinate_utils.py

Converts WGS84 lat/lon into the engine's local Cartesian coordinate system.

Convention (must match src/gis/CoordinateSystem in the C++ engine):
    x = local east-west distance in meters   (+x = east)
    y = elevation / height in meters         (+y = up)
    z = local north-south distance in meters (+z = north)

The origin (0, 0, 0) is the centroid of the Phase 1 study-area bounding box.
Raw latitude/longitude is never passed to the renderer -- every dataset in
data/processed/ is expressed in this local system.

We use an equirectangular (flat-earth) approximation. At the scale of a
single ~2 km^2 neighbourhood this introduces sub-centimeter error, which is
irrelevant for a hackathon MVP and far simpler than a full projected CRS.
"""

import json
import math
from pathlib import Path

EARTH_RADIUS_M = 6_371_000.0

REPO_ROOT = Path(__file__).resolve().parent.parent
BBOX_PATH = REPO_ROOT / "data" / "raw" / "study_area_bbox.json"


class LocalCoordinateSystem:
    """Projects lat/lon into local meters around a fixed origin (bbox centroid)."""

    def __init__(self, bbox: dict):
        self.lat_min = bbox["latitude_min"]
        self.lat_max = bbox["latitude_max"]
        self.lon_min = bbox["longitude_min"]
        self.lon_max = bbox["longitude_max"]

        self.origin_lat = (self.lat_min + self.lat_max) / 2.0
        self.origin_lon = (self.lon_min + self.lon_max) / 2.0

        # Precompute meters-per-degree at the origin latitude.
        self._m_per_deg_lat = (math.pi / 180.0) * EARTH_RADIUS_M
        self._m_per_deg_lon = (
            (math.pi / 180.0) * EARTH_RADIUS_M * math.cos(math.radians(self.origin_lat))
        )

    def to_local(self, lat: float, lon: float) -> tuple[float, float]:
        """Returns (x, z) in meters relative to the study-area centroid."""
        x = (lon - self.origin_lon) * self._m_per_deg_lon
        z = (lat - self.origin_lat) * self._m_per_deg_lat
        return x, z

    def bounds_local(self) -> dict:
        """Local-space extent of the study area, useful for camera framing / culling."""
        x_min, z_min = self.to_local(self.lat_min, self.lon_min)
        x_max, z_max = self.to_local(self.lat_max, self.lon_max)
        return {"x_min": x_min, "x_max": x_max, "z_min": z_min, "z_max": z_max}

    def contains(self, lat: float, lon: float) -> bool:
        return self.lat_min <= lat <= self.lat_max and self.lon_min <= lon <= self.lon_max


def load_study_area(bbox_path: Path = BBOX_PATH) -> LocalCoordinateSystem:
    with open(bbox_path, "r", encoding="utf-8") as f:
        bbox = json.load(f)
    return LocalCoordinateSystem(bbox)


def project_ring(coord_sys: LocalCoordinateSystem, ring_lonlat) -> list[list[float]]:
    """Converts a GeoJSON-style [ [lon, lat], ... ] ring into [[x, z], ...] local points."""
    return [list(coord_sys.to_local(lat, lon)) for lon, lat in ring_lonlat]


def polygon_centroid_xz(points_xz: list[list[float]]) -> tuple[float, float]:
    """Simple average-of-vertices centroid (fine for the small, mostly-convex OSM
    building footprints in this MVP; not a true area-weighted polygon centroid)."""
    n = len(points_xz)
    if n == 0:
        return 0.0, 0.0
    sx = sum(p[0] for p in points_xz)
    sz = sum(p[1] for p in points_xz)
    return sx / n, sz / n


if __name__ == "__main__":
    cs = load_study_area()
    print(f"Origin (lat, lon): {cs.origin_lat:.6f}, {cs.origin_lon:.6f}")
    print("Local bounds (meters):", cs.bounds_local())
