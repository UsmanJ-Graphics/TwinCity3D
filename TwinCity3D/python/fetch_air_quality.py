#!/usr/bin/env python3
"""
fetch_air_quality.py

Prototype data generation for Phase 26 (Air Quality)

This script produces data/processed/air_quality.json containing per-zone
air quality samples. It attempts no remote API by default (zones in this
project are in local meters and mapping to lat/lon/stations requires the
GIS preprocessing pipeline). Instead it generates deterministic, seeded
PM2.5 values per zone id so the C++ app can visualize and test the
Air Quality layer immediately.

Usage:
  python fetch_air_quality.py --out ../data/processed/air_quality.json

If you later have raw OpenAQ station data with lat/lon you can extend
this script to map stations to zone ids and aggregate measurements.
"""
import json
import os
import argparse
import math
from pathlib import Path

try:
	import requests
except Exception:
	requests = None

from coordinate_utils import load_study_area

def pm25_to_aqi(pm25):
	"""Convert PM2.5 (µg/m3) to a simplified AQI using EPA breakpoints.
	Uses the standard breakpoints with linear interpolation.
	"""
	# Breakpoints from US EPA
	breakpoints = [
		(0.0, 12.0, 0, 50),
		(12.1, 35.4, 51, 100),
		(35.5, 55.4, 101, 150),
		(55.5, 150.4, 151, 200),
		(150.5, 250.4, 201, 300),
		(250.5, 350.4, 301, 400),
		(350.5, 500.4, 401, 500),
	]
	for (clow, chigh, ilow, ihigh) in breakpoints:
		if pm25 >= clow and pm25 <= chigh:
			aqi = ((ihigh - ilow) / (chigh - clow)) * (pm25 - clow) + ilow
			return round(aqi)
	# If above 500.4, cap at 500
	return 500

def deterministic_pm25_for_zone(zone_id):
	# Deterministic pseudo-random generator based on zone id
	# Produces values roughly in a plausible urban PM2.5 range (5..120)
	seed = (zone_id * 9301 + 49297) % 233280
	r = seed / 233280.0
	# skew toward higher values for odd ids to provide variation
	base = 5.0 + r * 115.0
	if zone_id % 3 == 0:
		base *= 0.9
	if zone_id % 5 == 0:
		base *= 1.15
	return round(max(0.0, base), 1)

def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("--out", default="../data/processed/air_quality.json",
						help="output path for air_quality.json (relative to this script)")
	parser.add_argument("--zones", type=int, default=16,
						help="number of zones to synthesize (default 16)")
	parser.add_argument("--openaq", action="store_true", help="fetch recent OpenAQ PM2.5 measurements and map stations to zones")
	parser.add_argument("--stations", help="local stations JSON (array of {lat,lon,pm25}) to map to zones instead of synthetic values")
	args = parser.parse_args()

	out_path = os.path.join(os.path.dirname(__file__), args.out)
	os.makedirs(os.path.dirname(out_path), exist_ok=True)

	# If requested, try to fetch OpenAQ data (or consume local station file)
	samples = []  # list of (zone_id, pm25)

	repo_root = Path(__file__).resolve().parent.parent
	zones_path = repo_root / "data" / "processed" / "zones.json"
	coord_sys = None
	if args.openaq or args.stations:
		# load study area bbox for lat/lon -> local meters projection
		try:
			coord_sys = load_study_area()
		except Exception:
			coord_sys = None

	if args.stations:
		# local stations file expected as JSON array of {lat, lon, pm25}
		stations_path = os.path.join(os.path.dirname(__file__), args.stations)
		with open(stations_path, "r", encoding="utf-8") as f:
			stations = json.load(f)
		# map stations to zones by checking bounds_local in zones.json
		if zones_path.exists():
			with open(zones_path, "r", encoding="utf-8") as f:
				zones_doc = json.load(f)
			zones_bounds = {z["id"]: z["bounds_local"] for z in zones_doc.get("zones", [])}
			for s in stations:
				lat = s.get("lat") or s.get("latitude")
				lon = s.get("lon") or s.get("longitude")
				pm = s.get("pm25")
				if lat is None or lon is None or pm is None or coord_sys is None:
					continue
				x, z = coord_sys.to_local(lat, lon)
				# find containing zone
				for zid, b in zones_bounds.items():
					if b["x_min"] <= x <= b["x_max"] and b["z_min"] <= z <= b["z_max"]:
						samples.append((zid, float(pm)))
						break
	elif args.openaq and requests is not None:
		# Query OpenAQ latest measurements for PM2.5 within bbox
		bbox = None
		try:
			if zones_path.exists():
				with open(zones_path, "r", encoding="utf-8") as f:
					zones_doc = json.load(f)
				# derive bbox from study area via coordinate_utils origin bounds
				# fallback to requesting a broader area by not specifying bbox
				bbox = None
		except Exception:
			bbox = None

		# Fetch recent PM2.5 measurements (limit modestly)
		url = "https://api.openaq.org/v2/measurements"
		params = {"parameter": "pm25", "limit": 200, "sort": "desc"}
		try:
			resp = requests.get(url, params=params, timeout=10)
			resp.raise_for_status()
			j = resp.json()
			results = j.get("results", [])
			if results and coord_sys and zones_path.exists():
				with open(zones_path, "r", encoding="utf-8") as f:
					zones_doc = json.load(f)
				zones_bounds = {z["id"]: z["bounds_local"] for z in zones_doc.get("zones", [])}
				for r in results:
					coords = r.get("coordinates") or {}
					lat = coords.get("latitude")
					lon = coords.get("longitude")
					value = r.get("value")
					if lat is None or lon is None or value is None:
						continue
					x, z = coord_sys.to_local(lat, lon)
					for zid, b in zones_bounds.items():
						if b["x_min"] <= x <= b["x_max"] and b["z_min"] <= z <= b["z_max"]:
							samples.append((zid, float(value)))
							break
		except Exception as e:
			print("OpenAQ fetch failed:", e)

	# Aggregate samples per zone (mean) if any; otherwise fall back to synthetic
	agg = {}
	if samples:
		for zid, pm in samples:
			agg.setdefault(zid, []).append(pm)
		data = {"data_source": "openaq_aggregated" if args.openaq else "stations_aggregated", "zones": []}
		for zid, vals in agg.items():
			avg = sum(vals) / len(vals)
			data["zones"].append({"zone_id": int(zid), "pm25": round(avg, 1), "aqi": pm25_to_aqi(avg)})
	else:
		data = {"data_source": "synthetic_generated", "zones": []}
		for zid in range(args.zones):
			pm = deterministic_pm25_for_zone(zid)
			aqi = pm25_to_aqi(pm)
			data["zones"].append({"zone_id": zid, "pm25": pm, "aqi": aqi})

	with open(out_path, "w", encoding="utf-8") as f:
		json.dump(data, f, indent=2)

	print(f"Wrote {len(data['zones'])} zone samples to {out_path}")

if __name__ == "__main__":
	main()
