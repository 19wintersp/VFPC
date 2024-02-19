#pragma once

#include <atomic>
#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <utility>
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
		std::vector<Constraint> constraints;
		std::vector<Restriction> restrictions;
	};
}

// this class deals in multithreading, but its public APIs must only be called
// from one thread to ensure safety. this is acceptable in the current system.
class Source {
private:
	std::set<std::string> pending, missing, error;
	std::map<std::string, std::map<std::string, std::shared_ptr<api::Sid>>> sids;
	std::string web_source;

	std::pair<uint8_t, api::Time> datetime_value;

	std::atomic_uint cache_version;
	std::mutex cache_lock, update_lock;
	std::shared_mutex this_lock;

	void Source::fetch_update(std::promise<void> promise);
	void Source::fetch_airport(std::promise<void> promise, const char *icao);

public:
	enum CacheStatus {
		Extant,
		Pending,
		Missing,
		Error,
	};

	Source();
	~Source();

	void set(const char *source);
	void invalidate();
	void update();

	std::pair<uint8_t, api::Time> datetime();

	// this creates a TOCTOU issue, but invalidate is called from the same thread
	// as airport(...)/sid(...) so it's not an issue in practice. if necessary to
	// change this, we'll create an "Airport" type which references its SID map
	// and inherits lock_guard (?)
	CacheStatus airport(const char *icao);
	std::shared_ptr<const api::Sid> sid(const char *icao, const char *point);
};
