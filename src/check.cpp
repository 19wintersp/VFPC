#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "check.hpp"
#include "source.hpp"

Checker::Checker(Source &source) : source(source) {}

Result Checker::check(FlightPlan &fp, std::string *log) {
	Check check(fp, source);
	auto result = check.check();

	if (log) {
		log->append(check.log);

		switch (result) {
			case Result::Pending:
			case Result::Error:
				log->append("pending");
				break;

			case Result::Unknown:
			case Result::Success:
			case Result::Warning:
			case Result::NonIfr:
				log->append("pass");
				break;

			default:
				log->append("fail");
				break;
		}
	}

	return result;
}

Check::Check(FlightPlan &fp, Source &source) : fp(fp), source(source) {}

#define LOG(msg) { log.append(msg); log.append("; "); }

void to_upper(std::string &s) {
	std::transform(s.cbegin(), s.cend(), s.begin(), toupper);
}

const std::regex
	regex_csl(R"#(^(M\d{3}|[NK]\d{4})([FA]\d{3}|[SM]\d{4})$)#", std::regex::optimize),
	regex_clb(R"#(^(M\d{3}|[NK]\d{4})([FA]\d{3}|[SM]\d{4})([FA]\d{3}|[SM]\d{4}|PLUS)$)#", std::regex::optimize),
	regex_apt(R"#(^([A-Z]{4})(\/\d{2}[LCR]?)?$)#", std::regex::optimize),
	regex_ats(R"#(^([A-Z]{2,5}\d[A-Z]?|[USK]?[A-Z][1-9]\d{0,2}[A-Z]?|NAT[A-Z]|DCT)$)#", std::regex::optimize),
	regex_wpt(R"#(^([A-Z]{2,5}(\d{6})?|\d{2}(\d{2})?[SN]\d{3}(\d{2})?[WE])($|\/))#", std::regex::optimize);

Result Check::check() {
	if (!fp.is_ifr()) {
		LOG("plan type is not IFR");
		return Result::NonIfr;
	}

	const char *c_origin = fp.departure(), *c_destination = fp.destination();
	if (!c_origin || !c_origin[0] || !c_destination || !c_destination[0]) {
		LOG("origin/destination is missing");
		return Result::Syntax;
	}

	std::string origin(c_origin), destination(c_destination);
	to_upper(origin);
	to_upper(destination);

	switch (source.airport(origin.c_str())) {
		case Source::CacheStatus::Pending:
			LOG("loading data for origin");
			return Result::Pending;

		case Source::CacheStatus::Missing:
			LOG("server has no data for origin");
			return Result::Unknown;

		case Source::CacheStatus::Error:
			LOG("an error occurred when fetching data for origin");
			return Result::Error;

		case Source::CacheStatus::Extant:
			break;
	}

	const char *route_raw = fp.route();
	std::vector<std::string> route, points = fp.points();

	while (route_raw) {
		route_raw += strspn(route_raw, " ");
		if (!route_raw[0]) break;

		const char *next = strchr(route_raw, ' ');

		std::string segment;
		if (next) segment = std::string(route_raw, next - route_raw);
		else segment = std::string(route_raw);

		to_upper(segment);
		route.push_back(std::move(segment));

		route_raw = next;
	}

	if (route.empty()) {
		LOG("empty route");
		return Result::Syntax;
	}

	std::string sid = fp.sid_name(), sid_suffix, sid_point;
	to_upper(sid);

	if (!sid.empty()) {
		// remove prefix for legacy SIDs
		if (sid[0] == '#') sid.erase(0, 1);

		if (origin == "EGLL" && sid == "CHK") {
			// ridiculous, copied from old vFPC
			sid_suffix = "CHK";
			sid_point = "CPT";
		} else {
			sid_suffix = sid.back();
			sid_point = sid.substr(0, strcspn(sid.c_str(), "0123456789"));
		}
	}

	auto route_iter = route.cbegin();
	std::smatch match;

	if (std::regex_match(*route_iter, regex_csl)) route_iter++;

	if (route_iter == route.cend()) {
		LOG("route contains no route");
		return Result::Syntax;
	}

	if (std::regex_match(*route_iter, match, regex_apt)) {
		if (match[1].str() != origin) {
			LOG("route and flight plan origin do not match");
			return Result::Syntax;
		}

		route_iter++;
	}

	if (route_iter != route.cend() && std::regex_match(*route_iter, regex_csl))
		route_iter++;

	std::vector<std::string> bare_route;

	while (route_iter != route.cend()) {
		// really, this should be mandatory, but it is often omitted on VATSIM
		if (std::regex_match(*route_iter, regex_ats)) {
			if (*route_iter != "DCT") bare_route.push_back(*route_iter);
			route_iter++;
		}

		if (route_iter == route.cend()) break;
		if (std::regex_match(*route_iter, regex_apt)) break;

		bool climb = !(*route_iter).compare(0, 2, "C/");
		auto begin = (*route_iter).begin() + (climb ? 2 : 0);

		if (std::regex_search(begin, (*route_iter).end(), match, regex_wpt)) {
			bare_route.push_back(match[1].str());

			begin = match.suffix().first;

			if (begin != (*route_iter).end()) {
				if (!std::regex_match(begin, (*route_iter).end(), climb ? regex_clb : regex_csl)) {
					LOG("invalid change of speed/level");
					return Result::Syntax;
				}
			} else if (climb) {
				LOG("cruise climb does not include climb");
				return Result::Syntax;
			}

			route_iter++;
		} else break;

		// if we were to support changes of flight rules, it would be inserted here
	}

	if (route_iter != route.cend() && std::regex_match(*route_iter, match, regex_apt)) {
		if (match[1].str() != destination) {
			LOG("route and flight plan destination do not match");
			return Result::Syntax;
		}

		route_iter++;
	}

	if (route_iter != route.cend()) {
		LOG("invalid token in flight plan");
		return Result::Syntax;
	}

	auto sid_data = source.sid(origin.c_str(), sid_point.c_str());
	if (!sid_data) {
		LOG("departure not in database");
		return Result::SidUnknown;
	}

	return check_constraints(*sid_data, sid_point.c_str(), sid_suffix.c_str(), points, bare_route);
}

struct Candidate {
	const api::Constraint &constraint;

	short passes = 0;
	Result result = Result::Success;
	std::string log;

	Candidate(const api::Constraint &constraint) : constraint(constraint) {}
};

Result Check::check_constraints(
	const api::Sid &sid_data,
	const char *sid_point,
	const char *sid_suffix,
	const std::vector<std::string> &points,
	const std::vector<std::string> &route
) {
	Result sr_result = check_restrictions(sid_data.restrictions, sid_suffix);

	// this is messy; constraints are checked in turn, and the one with the most
	// passes (ordered semantically) is selected as the canonical failure. since
	// the log pointer is global (meh) some nonsense is required.

	std::vector<Candidate> candidates;
	for (const auto &constraint : sid_data.constraints)
		candidates.push_back(std::move(Candidate(constraint)));

	#define CHECK(check) \
		candidate.result = check; \
		if (candidate.result == Result::Success) candidate.passes++; \
		else { log.swap(candidate.log); continue; }

	for (auto &candidate : candidates) {
		log.swap(candidate.log);

		CHECK(check_destination(candidate.constraint, fp.destination()));
		CHECK(check_exit_point(candidate.constraint, points));

		// the following checks most likely should not be used for ranking

		Result sr_result_copy = sr_result;
		candidate.result = check_restrictions(
			candidate.constraint.restrictions, sid_suffix, sr_result_copy
		);

		if (candidate.result != Result::Success) {
			log_alternatives(candidate.constraint.restrictions);
			continue;
		}

		if (sr_result_copy != Result::Success) {
			log_alternatives(sid_data.restrictions);
			candidate.result = sr_result_copy;
			continue;
		}

		candidate.passes++;

		CHECK(check_min_max(candidate.constraint, fp.cruise_level()));
		CHECK(check_direction(candidate.constraint, fp.cruise_level()));
		CHECK(check_route(candidate.constraint, route, sid_point));
		CHECK(check_alerts(candidate.constraint));

		log.swap(candidate.log);
		log.append(candidate.log);

		LOG("candidate constraint passed");
		return Result::Success;
	}

	#undef CHECK

	const Candidate &best = *std::max_element(
		candidates.cbegin(), candidates.cend(),
		[](const auto &a, const auto &b) { return a.passes < b.passes; }
	);

	log.append("best candidate for SID: ");
	log.append(best.log);
	return best.result;
}

Result Check::check_destination(const api::Constraint &constraint, const char *dest) {
	auto predicate = [dest](const std::string &slug) {
		return !strncmp(slug.c_str(), dest, slug.length());
	};

	if (
		!constraint.nodests.empty() &&
		std::any_of(constraint.nodests.begin(), constraint.nodests.end(), predicate)
	) {
		LOG("destination matches blacklist");
		return Result::Destination;
	}

	if (
		!constraint.dests.empty() &&
		std::none_of(constraint.dests.begin(), constraint.dests.end(), predicate)
	) {
		LOG("destination not in whitelist");
		return Result::Destination;
	}

	return Result::Success;
}

Result Check::check_exit_point(const api::Constraint &constraint, const std::vector<std::string> &points) {
	auto predicate = [points](const std::string &exit_point) {
		return std::find(points.begin(), points.end(), exit_point) != points.end();
	};

	if (
		!constraint.nopoints.empty() &&
		std::any_of(constraint.nopoints.begin(), constraint.nopoints.end(), predicate)
	) {
		LOG("exit point matches blacklist");
		return Result::ExitPoint;
	}

	if (
		!constraint.points.empty() &&
		std::none_of(constraint.points.begin(), constraint.points.end(), predicate)
	) {
		LOG("exit point not in whitelist");
		return Result::ExitPoint;
	}

	return Result::Success;
}

Result Check::check_min_max(const api::Constraint &constraint, int rfl) {
	rfl /= 100;

	if (constraint.min && *constraint.min > rfl) {
		LOG("requested level beneath minimum");
		return Result::LevelBlock;
	}

	if (constraint.max && *constraint.max < rfl) {
		LOG("requested level above maximum");
		return Result::LevelBlock;
	}

	return Result::Success;
}

const int RVSM_START = 41;

Result Check::check_direction(const api::Constraint &constraint, int rfl) {
	if (rfl % 1000) {
		LOG("requested level not IFR");
		return Result::LevelSeries;
	}

	if (constraint.dir) {
		rfl /= 1000;

		if (rfl <= RVSM_START) {
			if (rfl % 2 != (int) *constraint.dir) {
				LOG("requested level has incorrect parity");
				return Result::LevelParity;
			}
		} else {
			if ((2 + rfl - RVSM_START) % 4 != (int) *constraint.dir * 2) {
				LOG("requested RVSM level has incorrect parity");
				return Result::LevelParity;
			}
		}
	}

	return Result::Success;
}

Result Check::check_route(
	const api::Constraint &constraint,
	const std::vector<std::string> &route,
	const char *sid_point
) {
	auto predicate = [route, sid_point](const std::string &candidate) {
		if (candidate == "*") return true;

		std::vector<std::string> candidate_route;
		auto cursor = candidate.cbegin(), start = cursor;

		do {
			cursor = std::find(cursor, candidate.cend(), ' ');
			candidate_route.push_back(std::string(start, cursor));
		} while (cursor != candidate.cend() && (start = ++cursor) != candidate.cend());

		if (candidate_route.empty() != route.empty()) return false;

		auto ccursor = candidate_route.cbegin(), rcursor = route.cbegin();

		if (*sid_point) {
			// skip the parts before the first common waypoint, or reject if disjoint
			while (rcursor != route.cend() && *rcursor != candidate_route[0]) rcursor++;
			if (rcursor == route.cend()) return false;
		}

		while (ccursor != candidate_route.cend()) {
			if (rcursor == route.cend()) return false;
			if (*ccursor != *rcursor && strcmp(ccursor->c_str(), "*")) return false;

			ccursor++; rcursor++;
		}

		return true;
	};

	if (
		!constraint.noroute.empty() &&
		std::any_of(constraint.noroute.begin(), constraint.noroute.end(), predicate)
	) {
		LOG("route matches blacklist");
		return Result::Route;
	}

	if (
		!constraint.route.empty() &&
		std::none_of(constraint.route.begin(), constraint.route.end(), predicate)
	) {
		LOG("route not in whitelist");
		return Result::Route;
	}

	return Result::Success;
}

Result Check::check_alerts(const api::Constraint &constraint) {
	Result result = Result::Success;

	for (const api::Alert &alert : constraint.alerts) {
		if (alert.ban) {
			std::string message("candidate is banned");
			if (alert.note) {
				message.append(": ");
				message.append(alert.note->c_str());
			}

			LOG(message.c_str());
			return Result::CstrBan;
		}

		if (alert.warn) {
			std::string message("candidate contains warning");
			if (alert.note) {
				message.append(": ");
				message.append(alert.note->c_str());
			}

			LOG(message.c_str());
			result = Result::Warning;
		}
	}

	return result;
}

Result Check::check_restrictions(
	const std::vector<api::Restriction> &restrictions,
	const char *sid_suffix
) {
	Result unused; // for the "sidlevel" override; not used for SID-wide restrs
	return check_restrictions(restrictions, sid_suffix, unused);
}

Result Check::check_restrictions(
	const std::vector<api::Restriction> &restrictions,
	const char *sid_suffix,
	Result &sr_result
) {
	if (restrictions.empty()) return Result::Success;

	auto datetime = source.datetime();

	for (const api::Restriction &restriction : restrictions) {
		if (restriction.start && restriction.end && datetime.time) {
			if (restriction.start->date && restriction.end->date) {
				bool time_check[2];

				uint8_t
					date_min = std::min(*restriction.start->date, *restriction.end->date),
					date_max = std::max(*restriction.start->date, *restriction.end->date);
				bool
					wrap = *restriction.start->date > *restriction.end->date,
					cont = date_min < *datetime.date && *datetime.date < date_max;

				if (date_min == *datetime.date) time_check[wrap] = true;
				if (date_max == *datetime.date) time_check[1 - wrap] = true;

				if (wrap == cont && !time_check[0] && !time_check[1]) continue;

				if (restriction.start->time && restriction.end->time) {
					uint16_t
						time_start = restriction.start->time->ord(),
						time_end   = restriction.end->time->ord();

					if (time_check[0] && time_start > datetime.time->ord()) continue;
					if (time_check[1] && time_end   < datetime.time->ord()) continue;
				}
			} else if (restriction.start->time && restriction.end->time) {
				uint16_t
					time_start = restriction.start->time->ord(),
					time_end   = restriction.end->time->ord(),
					time_min = std::min(time_start, time_end),
					time_max = std::max(time_start, time_end);
				bool
					wrap = time_start > time_end,
					cont = time_min < datetime.time->ord() && datetime.time->ord() < time_max;

				if (wrap == cont) continue;
			}
		}

		if (!restriction.suffix.empty()) {
			if (
				std::none_of(
					restriction.suffix.begin(), restriction.suffix.end(),
					[sid_suffix](const std::string &suffix) {
						if (suffix.length() > strlen(sid_suffix)) return false;

						size_t offset = strlen(sid_suffix) - suffix.length();
						return !strcmp(suffix.c_str(), sid_suffix + offset);
					}
				)
			) continue;
		}

		if (!restriction.types.empty()) {
			if (
				std::find(
					restriction.types.begin(), restriction.types.end(),
					std::string(1, fp.engine_type())
				) == restriction.types.end() &&
				std::find(
					restriction.types.begin(), restriction.types.end(),
					std::string(1, fp.aircraft_type())
				) == restriction.types.end()
			) continue;
		}

		if (restriction.banned) {
			LOG("banned condition matches");
			return Result::CondBan;
		}

		if (restriction.sidlevel && sr_result != Result::Success) {
			LOG("overridden by matching constrained condition");
			sr_result = Result::Success;
		}

		return Result::Success;
	}

	LOG("no conditions match");
	return Result::CondFail;
}

void Check::log_alternatives(const std::vector<api::Restriction> &restrictions) {
	std::string alternatives("alternatives exist (");
	auto length = alternatives.length();

	for (const api::Restriction &restriction : restrictions) {
		for (const std::string &alternative : restriction.alt) {
			if (alternatives.back() != '(') alternatives.append(", ");
			alternatives.append(alternative);
		}
	}

	if (alternatives.length() > length) {
		alternatives.push_back(')');
		LOG(alternatives);
	}
}
