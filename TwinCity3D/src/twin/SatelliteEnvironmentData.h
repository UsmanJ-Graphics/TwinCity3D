#pragma once
#include <string>
#include <vector>

namespace twin {
    struct SatelliteEnvironmentSample { int zoneId{-1}; float vegetationIndex{0.0f}; float builtUpIndex{0.0f}; };
    struct SatelliteEnvironmentData {
        std::vector<SatelliteEnvironmentSample> zones;
        std::string dataSource;
        std::string method;
        bool valid{false};
        bool IsSatelliteProcessed() const { return dataSource == "satellite_processed"; }
    };
}
