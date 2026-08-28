#pragma once
#include "SatelliteEnvironmentData.h"
#include <string>
namespace twin { class SatelliteEnvironmentLoader { public: static bool Load(const std::string& path, SatelliteEnvironmentData& out); }; }
