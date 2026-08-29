#pragma once
#include <string>
#include <vector>

namespace twin {

	enum class ReportCategory {
		Heat,
		Flood,
		Waste,
		BrokenRoad,
		Drainage,
		Pollution,
		Other
	};

	enum class ReportStatus {
		New,
		Verified,
		InProgress,
		Resolved
	};

	struct CitizenReport {
		int id{ -1 };
		int zoneId{ -1 }; // optional: zone id the report refers to
		float localX{ 0.0f }; // local x,z meters (optional precise location)
		float localZ{ 0.0f };
		ReportCategory category{ ReportCategory::Other };
		ReportStatus status{ ReportStatus::New };
		std::string timestamp; // ISO8601 or human string
		std::string description;
	};

} // namespace twin
