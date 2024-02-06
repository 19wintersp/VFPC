#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace api {
	struct Time {
		uint8_t hour, minute;
	};

	enum class Direction {
		Odd  = 1,
		Even = 0,
	};

	struct Alert {
		bool ban, warn;
		std::optional<std::string> note;
		std::optional<uint32_t> srd;
	};

	struct RestrictionTime {
		std::optional<uint8_t> date;
		std::optional<Time> time;
	};

	struct Restriction {
		bool sidlevel, banned;
		std::vector<std::string> alt, suffix, types;
		std::optional<RestrictionTime> start, end;
	};

	struct Constraint {
		std::optional<uint16_t> min, max;
		std::optional<Direction> dir;
		std::vector<std::string> dests, nodests, points, nopoints, route, noroute;
		std::vector<Alert> alerts;
		std::vector<Restriction> restrictions;
	};

	struct Sid {
		std::string &point;
		std::vector<Constraint> constraints;
		std::vector<Restriction> restrictions;
	};
}
