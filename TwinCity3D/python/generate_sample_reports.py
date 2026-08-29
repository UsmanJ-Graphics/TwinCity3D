#!/usr/bin/env python3
"""
generate_sample_reports.py

Create a small sample data/processed/citizen_reports.json for Phase 27
"""
import json
import os
from datetime import datetime

out = os.path.join(os.path.dirname(__file__), "../data/processed/citizen_reports.json")
os.makedirs(os.path.dirname(out), exist_ok=True)

reports = {
	"reports": [
		{"id": 0, "zone_id": 2, "local_x": 0.0, "local_z": 0.0, "category": 0, "status": 0, "timestamp": datetime.utcnow().isoformat() + "Z", "description": "Heat distress reported near market."},
		{"id": 1, "zone_id": 7, "local_x": 0.0, "local_z": 0.0, "category": 5, "status": 0, "timestamp": datetime.utcnow().isoformat() + "Z", "description": "Illegal dumping observed."},
		{"id": 2, "zone_id": 11, "local_x": 0.0, "local_z": 0.0, "category": 3, "status": 1, "timestamp": datetime.utcnow().isoformat() + "Z", "description": "Collapsed road surface reported."}
	]
}

with open(out, "w", encoding="utf-8") as f:
	json.dump(reports, f, indent=2)

print(f"Wrote {len(reports['reports'])} sample reports to {out}")
