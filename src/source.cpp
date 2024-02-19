#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <optional>
#include <thread>
#include <vector>

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <ca-bundle.h>
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

static json fetch(const char *url);
static void load(
	std::vector<api::Airport> &airports,
	std::map<std::string, std::map<std::string, std::shared_ptr<api::Sid>>> &sids
);

Source::Source() :
	web_source(DEFAULT_SOURCE),
	cache_version(0)
{
	update();
}

Source::~Source() {
	// should cause any subsequently-finishing threads to fail
	cache_version++;

	spdlog::trace("acquiring source locks");

	this_lock.lock();
	cache_lock.lock();
	update_lock.lock();

	spdlog::trace("source destroyed");
}

void Source::set(const char *source) {
	if (!source) {
		spdlog::trace("resetting source");

		web_source = DEFAULT_SOURCE;
	} else if (strstr(source, "://")) {
		spdlog::trace("setting new web source");

		web_source = source;
		if (web_source.back() != '/') web_source.push_back('/');
	} else {
		spdlog::trace("loading file source");

		std::ifstream fd(source);
		std::vector<api::Airport> data = json::parse(fd);

		std::lock_guard<std::mutex> _lock(cache_lock);
		load(data, sids);
	}
}

void Source::invalidate() {
	cache_version++;

	std::lock_guard<std::mutex> _lock(cache_lock);

	pending.clear();
	missing.clear();
	error.clear();
	sids.clear();
}

void Source::update() {
	spdlog::trace("update triggered");

	std::promise<void> promise;
	std::future<void> future = promise.get_future();

	std::thread t(&Source::fetch_update, this, std::move(promise));
	t.detach();

	future.wait();
}

void Source::fetch_update(std::promise<void> promise) {
	std::string url = web_source; // copy
	url.append("version");

	std::shared_lock<std::shared_mutex> _lock(this_lock);

	// we have to take the lock on our worker thread, but we shouldn't allow the
	// main thread to continue until this lock is created. hence, signal.
	// because this is required, we can take advantage of it to get our other
	// variables too.
	promise.set_value();

	spdlog::trace("updating");

	api::Version version;
	try {
		version = fetch(url.c_str());
	} catch (...) {
		spdlog::warn("update failed"); // TODO: fixme
		return;
	}

	std::lock_guard<std::mutex> _lock2(update_lock);

	datetime_value = std::make_pair(version.day, version.time);

	spdlog::trace("update complete");
}

std::pair<uint8_t, api::Time> Source::datetime() {
	std::lock_guard<std::mutex> _lock(update_lock);
	return datetime_value;
}

Source::CacheStatus Source::airport(const char *icao) {
	std::lock_guard<std::mutex> _lock(cache_lock);

	if (  error.find(icao) !=   error.end()) return Source::CacheStatus::Error;
	if (missing.find(icao) != missing.end()) return Source::CacheStatus::Missing;
	if (pending.find(icao) != pending.end()) return Source::CacheStatus::Pending;
	if (   sids.find(icao) !=    sids.end()) return Source::CacheStatus::Extant;

	// TODO: we could batch these requests, and have fetch_airport wait and then
	// fetch all in pending

	pending.insert(std::string(icao));

	std::promise<void> promise;
	std::future<void> future = promise.get_future();

	std::thread t(&Source::fetch_airport, this, std::move(promise), icao);
	t.detach();

	future.wait();

	return Source::CacheStatus::Pending;
}

void Source::fetch_airport(std::promise<void> promise, const char *c_icao) {
	spdlog::trace("requesting airport {}", c_icao);

	std::string url = web_source, icao = c_icao; // copy
	url.append("airport?icao=");
	url.append(c_icao); // URL-encoding shouldn't be an issue

	auto initial_cache_version = cache_version.load();

	std::shared_lock<std::shared_mutex> _lock(this_lock);

	// see Source::fetch_update
	promise.set_value();

	std::vector<api::Airport> airports;
	try {
		airports = fetch(url.c_str());
	} catch (...) {
		spdlog::warn("airport request failed"); // TODO: fixme

		std::lock_guard<std::mutex> _lock2(cache_lock);

		pending.erase(icao);

		try {
			throw;
		} catch (int code) {
			if (code == 404) {
				missing.insert(icao);
				return;
			}
		} catch (...) {}

		error.insert(icao);

		return;
	}

	if (initial_cache_version != cache_version.load()) {
		spdlog::trace("discarding fetch result due to cache invalidation");
		return;
	}

	std::lock_guard<std::mutex> _lock2(cache_lock);

	load(airports, sids);
	pending.erase(icao);

	spdlog::trace("airport request complete");
}

std::shared_ptr<const api::Sid> Source::sid(const char *icao, const char *point) {
	std::lock_guard<std::mutex> _lock(cache_lock);

	auto airport_it = sids.find(icao);
	if (airport_it == sids.end()) return nullptr;

	auto &airport = std::get<1>(*airport_it);
	auto sid_it = airport.find(point);
	if (sid_it == airport.end()) return nullptr;

	return std::get<1>(*sid_it);
}

static char *user_agent = nullptr;
static char user_agent_buffer[64] = PLUGIN_NAME "/" PLUGIN_VERSION " ";

static size_t fetch_write(char *data, size_t size, size_t nmemb, void *user) {
	size_t n = size * nmemb;
	((std::string *) user)->append(data, n);
	return n;
}

static json fetch(const char *url) {
	spdlog::trace("fetch {}", url);

	CURL *curl = curl_easy_init(); // TODO: should we re-use this handle?
	if (!curl) throw std::string("failed to init libcurl easy");

	if (!user_agent) {
		const char *curl_ua = curl_version();
		strncat(user_agent_buffer, curl_ua, strcspn(curl_ua, " "));

		user_agent = user_agent_buffer;
	}

	char error[CURL_ERROR_SIZE];
	std::string buffer; // TODO: stream into the JSON parser instead?

	struct curl_blob ca_info;
	ca_info.data = (char *) CA_BUNDLE;
	ca_info.len = strlen((const char *) ca_info.data);
	ca_info.flags = CURL_BLOB_COPY;

	curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &ca_info); // see #2
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, (long) 0); // see #1
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, (long) 1);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long) 20);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fetch_write);

	CURLcode ret = curl_easy_perform(curl);

	long code;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);

	curl_easy_cleanup(curl);

	if (ret != CURLE_OK) throw std::string(error);
	if (code < 200 || code >= 300) {
		/* std::snprintf(error, CURL_ERROR_SIZE, "server returned code %ld", code);
		throw std::string(error); */

		throw (int) code;
	}

	json out = json::parse(buffer);
	return out;
}

static void load(
	std::vector<api::Airport> &airports,
	std::map<std::string, std::map<std::string, std::shared_ptr<api::Sid>>> &sids
) {
	for (auto &airport : airports) {
		spdlog::debug("adding {}", airport.icao.c_str());

		auto &sid_map = sids[std::move(airport.icao)];

		for (auto &sid_raw : airport.sids) {
			auto ptr = std::make_shared<api::Sid>();
			ptr->constraints = std::move(sid_raw.constraints);
			ptr->restrictions = std::move(sid_raw.restrictions);

			spdlog::debug("-> {}", sid_raw.point.c_str());

			sid_map.insert_or_assign(std::move(sid_raw.point), ptr);
			for (auto &alias : sid_raw.aliases)
				sid_map.insert_or_assign(std::move(alias), ptr);
		}
	}
}
