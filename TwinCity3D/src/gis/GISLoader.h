#pragma once
#include "GISTypes.h"
#include <string>

namespace twin::gis {

// Reads data/processed/{buildings,roads,green_areas,facilities,zones}.json
// (produced by python/preprocess_osm.py or python/build_sample_data.py)
// into an in-memory GISDataset. Pure data I/O — no OpenGL calls here, so it
// can be unit-tested without a graphics context (Critical Engineering Rule
// #5: keep data acquisition separate from rendering).
class GISLoader {
public:
    // dataDir is the folder containing the five processed JSON files
    // (typically "data/processed"). Returns false only if buildings.json
    // (the minimum needed to show anything) can't be read; missing
    // optional files (e.g. facilities.json) just leave that vector empty.
    static bool Load(const std::string& dataDir, GISDataset& outDataset);
};

}  // namespace twin::gis
