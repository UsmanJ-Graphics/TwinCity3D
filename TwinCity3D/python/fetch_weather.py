#!/usr/bin/env python3
"""
Phase 5, Step 1 — Weather data ingestion (Open-Meteo).

Live-first, fallback-second: same pattern as Phase 2's OSM ingestion.

1. Try a live request to api.open-meteo.com for the study-area centroid.
2. If that fails for any reason (no network, non-200, timeout, bad JSON),
   fall back to a clearly-labeled synthetic reading.
3. Always write data/processed/weather.json, always stamp
   _meta.data_source so the C++ side (WeatherLoader) and the UI can tell
   real data from sample data - Critical Engineering Rule #2: never
   disguise sample data as real.
"""

import json
import math
import os
import sys
import urllib.error
import urllib.request
from datetime import datetime, timezone

STUDY_AREA_LAT = 31.5180
STUDY_AREA_LON = 74.3503

OUTPUT_PATH = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "data", "processed", "weather.json",
)

OPEN_METEO_URL = (
    "https://api.open-meteo.com/v1/forecast"
    f"?latitude={STUDY_AREA_LAT}&longitude={STUDY_AREA_LON}"
    "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
    "precipitation,wind_speed_10m"
    "&hourly=temperature_2m"
    "&forecast_days=1"
    "&timezone=Asia%2FKarachi"
)

# Clearly-labeled fallback reading - a plausible late-August Lahore
# afternoon, NOT a live measurement. See _meta.data_source in the output.
FALLBACK_CURRENT = {
    "temperature_c": 34.6,
    "apparent_temperature_c": 38.1,
    "humidity_pct": 55,
    "precipitation_mm": 0.0,
    "wind_speed_kmh": 9.4,
}


def build_fallback_hourly():
    """24 synthetic hourly samples on a normal diurnal curve, centered on
    FALLBACK_CURRENT, so the fallback path has a self-consistent 24h
    array rather than a flat line."""
    hours = []
    base = FALLBACK_CURRENT["temperature_c"]
    now = datetime.now(timezone.utc)
    for h in range(24):
        swing = 6.0 * math.sin((h - 5) / 24.0 * 2 * math.pi - math.pi / 2)
        temp = round(base - 3.0 + swing, 1)
        hours.append({
            "time": f"{now.strftime('%Y-%m-%d')}T{h:02d}:00",
            "temperature_c": temp,
        })
    return hours


def fetch_live():
    """Attempt a live Open-Meteo request. Returns a dict on success,
    raises on any failure (caller decides how to fall back)."""
    req = urllib.request.Request(OPEN_METEO_URL, headers={"User-Agent": "LahoreDigitalTwin/0.1"})
    with urllib.request.urlopen(req, timeout=8) as resp:
        if resp.status != 200:
            raise RuntimeError(f"HTTP {resp.status}")
        payload = json.loads(resp.read().decode("utf-8"))

    current = payload["current"]
    hourly_times = payload["hourly"]["time"]
    hourly_temps = payload["hourly"]["temperature_2m"]

    return {
        "current": {
            "temperature_c": current["temperature_2m"],
            "apparent_temperature_c": current["apparent_temperature"],
            "humidity_pct": current["relative_humidity_2m"],
            "precipitation_mm": current["precipitation"],
            "wind_speed_kmh": current["wind_speed_10m"],
        },
        "hourly": [
            {"time": t, "temperature_c": temp}
            for t, temp in zip(hourly_times, hourly_temps)
        ],
        "data_source": "open_meteo_live",
    }


def build_fallback():
    return {
        "current": dict(FALLBACK_CURRENT),
        "hourly": build_fallback_hourly(),
        "data_source": "sample_fallback",
    }


def main():
    try:
        result = fetch_live()
        print(f"[fetch_weather] live Open-Meteo request succeeded "
              f"({len(result['hourly'])} hourly samples)")
    except Exception as e:
        print(f"[fetch_weather] live request failed ({e.__class__.__name__}: {e}), "
              f"falling back to sample_fallback", file=sys.stderr)
        result = build_fallback()

    output = {
        "current": result["current"],
        "hourly": result["hourly"],
        "_meta": {
            "data_source": result["data_source"],
            "fetched_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "study_area_centroid": {"lat": STUDY_AREA_LAT, "lon": STUDY_AREA_LON},
        },
    }

    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w") as f:
        json.dump(output, f, indent=2)

    print(f"[fetch_weather] wrote {OUTPUT_PATH} "
          f"(data_source={output['_meta']['data_source']})")


if __name__ == "__main__":
    main()
