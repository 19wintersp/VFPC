#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include <config.h>
#include "jsonify.hpp"
#include "source.hpp"

#define DEFAULT_SOURCE "https://vfpc.tomjmills.co.uk/"

using json = nlohmann::json;

namespace api {
	void from_json(const json &j, Time &time) {
		// really horrible conversion, supports %H:%M[:%S] and %H%M used by the API

		const char *str = j.get_ref<const json::string_t &>().c_str();

		if (strlen(str) < 4) return;
		char hour[3] = { str[0], str[1], 0 };

		str += 2;
		if (*str == ':') str++;

		if (strlen(str) < 2) return;
		char minute[3] = { str[0], str[1], 0 };

		time.hour = strtol(hour, nullptr, 10);
		time.minute = strtol(minute, nullptr, 10);
	}

	NLOHMANN_JSON_SERIALIZE_ENUM(Direction, {
		{ Direction::Odd,	"ODD" },
		{ Direction::Even, "EVEN" },
	});

	NLOHMANN_JSONIFY_DESERIALIZE_STRUCT(Alert, ban, warn, note, srd);
	NLOHMANN_JSONIFY_DESERIALIZE_STRUCT(RestrictionTime, date, time);
	NLOHMANN_JSONIFY_DESERIALIZE_STRUCT(Restriction, sidlevel, banned, alt, suffix, types, start, end);
	NLOHMANN_JSONIFY_DESERIALIZE_STRUCT(Constraint, min, max, dir, dests, nodests, points, nopoints, route, noroute, alerts, restrictions);

	struct Version {
		std::string api_version;
		Time time;
		uint8_t day;
	};

	NLOHMANN_JSONIFY_DESERIALIZE_STRUCT(Version, api_version, time, day);

	struct SidRaw {
		std::string point;
		std::vector<std::string> aliases;
		std::vector<Constraint> constraints;
		std::vector<Restriction> restrictions;
	};

	NLOHMANN_JSONIFY_DESERIALIZE_STRUCT(SidRaw, point, aliases, constraints, restrictions);

	struct Airport {
		std::string icao;
		std::vector<SidRaw> sids;
	};

	NLOHMANN_JSONIFY_DESERIALIZE_STRUCT(Airport, icao, sids);
}
