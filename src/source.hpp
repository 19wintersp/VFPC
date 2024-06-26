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

#include <nlohmann/json.hpp>

namespace api {
	struct Time {
		uint8_t hour, minute;

		uint16_t ord() const {
			return (uint16_t) hour * 60 + (uint16_t) minute;
		}
	};

	struct DateTime {
		std::optional<uint8_t> date;
		std::optional<Time> time;
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

	struct Restriction {
		bool sidlevel, banned;
		std::vector<std::string> alt, suffix, types;
		std::optional<DateTime> start, end;
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

class Source {
public:
	enum CacheStatus {
		Extant,
		Pending,
		Missing,
		Error,
	};

	virtual api::DateTime datetime() {
		return {};
	}

	virtual CacheStatus airport(const char *_icao) {
		return CacheStatus::Missing;
	}

	virtual std::shared_ptr<const api::Sid> sid(const char *_icao, const char *_point) {
		return nullptr;
	}
};

#ifndef VFPC_STANDALONE
// this class deals in multithreading, but its public APIs must only be called
// from one thread to ensure safety. this is acceptable in the current system.
class PluginSource : public virtual Source {
private:
	std::set<std::string> pending, missing, error;
	std::map<std::string, std::map<std::string, std::shared_ptr<api::Sid>>> sids;
	std::string web_source;

	api::DateTime datetime_value;

	std::atomic_uint cache_version;
	std::mutex cache_lock, update_lock;
	std::shared_mutex this_lock;

	void fetch_update(std::promise<void> promise);
	void fetch_airport(std::promise<void> promise, const char *icao);

public:
	PluginSource();
	~PluginSource();

	void set(const char *source);
	void invalidate();
	void update();

	api::DateTime datetime() override;

	// this creates a TOCTOU issue, but invalidate is called from the same thread
	// as airport(...)/sid(...) so it's not an issue in practice. if necessary to
	// change this, we'll create an "Airport" type which references its SID map
	// and inherits lock_guard (?)
	Source::CacheStatus airport(const char *icao) override;
	std::shared_ptr<const api::Sid> sid(const char *icao, const char *point) override;
};
#endif // ifndef VFPC_STANDALONE

class StaticSource : public virtual Source {
private:
	api::DateTime datetime_value;
	std::map<std::string, std::map<std::string, std::shared_ptr<api::Sid>>> sids;

public:
	StaticSource(nlohmann::json &data, api::DateTime datetime);

	api::DateTime datetime() override;

	Source::CacheStatus airport(const char *icao) override;
	std::shared_ptr<const api::Sid> sid(const char *icao, const char *point) override;
};
