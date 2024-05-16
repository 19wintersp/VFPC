#include "flightplan.hpp"

#include <cstring>
#include <ctime>

#ifndef VFPC_STANDALONE
EuroScopeFlightPlan::EuroScopeFlightPlan(const FlightPlan &fp) : fp(fp) {}

bool EuroScopeFlightPlan::is_ifr() {
	return fp.GetFlightPlanData().GetPlanType()[0] == 'I';
}

const char *EuroScopeFlightPlan::departure() {
	return fp.GetFlightPlanData().GetOrigin();
}

const char *EuroScopeFlightPlan::destination() {
	return fp.GetFlightPlanData().GetDestination();
}

int EuroScopeFlightPlan::cruise_level() {
	return fp.GetFlightPlanData().GetFinalAltitude();
}

const char *EuroScopeFlightPlan::route() {
	return fp.GetFlightPlanData().GetRoute();
}

std::vector<std::string> EuroScopeFlightPlan::points() {
	auto exroute = fp.GetExtractedRoute();
	std::vector<std::string> ret;

	for (int i = 0; i < exroute.GetPointsNumber(); i++)
		ret.push_back(std::string(exroute.GetPointName(i)));

	return ret;
}

const char *EuroScopeFlightPlan::sid_name() {
	return fp.GetFlightPlanData().GetSidName();
}

char EuroScopeFlightPlan::engine_type() {
	return fp.GetFlightPlanData().GetEngineType();
}

char EuroScopeFlightPlan::aircraft_type() {
	return fp.GetFlightPlanData().GetAircraftType();
}
#endif // ifndef VFPC_STANDALONE

#ifdef VFPC_STANDALONE
static void confirm(bool condition, const char *msg = "bad format") {
	if (!condition) throw msg;
}

static void next(const char **fp, const char **end) {
	confirm(*end);

	*fp = *end + 1;
	*end = strchr(*fp, '-');
}

static IcaoAircraft *search_icao(const char *icao, size_t icao_len) {
	IcaoAircraft *start = ICAO_AIRCRAFT, *pivot;
	size_t span = ICAO_AIRCRAFT_LEN;

	while (span > 8) {
		pivot = start + (span >> 1);
		if (strncmp(icao, pivot->icao, icao_len) < 0) {
			span >>= 1;
		} else {
			start = pivot;
			span = span - (span >> 1);
		}
	}

	for (IcaoAircraft *aircraft = start; aircraft < start + span; aircraft++)
		if (!strncmp(aircraft->icao, icao, icao_len))
			return aircraft;

	return nullptr;
}

IcaoFlightPlan::IcaoFlightPlan(const char *fp) {
	confirm(!strncmp(fp, "(FPL-", 5));

	const char *end = fp + 4, *trunc, *sub;

	next(&fp, &end);

	callsign_ = std::string(fp, end - fp);

	next(&fp, &end);

	confirm(end > fp);
	ifr_ = fp[0] == 'I';

	next(&fp, &end);

	trunc = strchr(fp, '/');
	confirm(trunc && trunc < end);
	aircraft = search_icao(fp, trunc - fp);
	confirm(aircraft, "unknown aircraft");

	next(&fp, &end);
	next(&fp, &end);

	confirm(end - fp >= 8);
	departure_ = std::string(fp, 4);
	dof_eobt_.time.emplace();
	dof_eobt_.time->hour   = std::stoi(std::string(fp + 4, 2));
	dof_eobt_.time->minute = std::stoi(std::string(fp + 6, 2));

	next(&fp, &end);

	trunc = strchr(fp, ' ');
	confirm(trunc && trunc < end);

	sub = strpbrk(fp, "AF") + 1;
	confirm(sub < trunc);
	cruise_level_ = std::stoi(std::string(sub, trunc - sub)) * 100;

	sub = trunc + 1;
	route_ = std::string(sub, end - sub);

	trunc = strchr(sub, ' ');
	if (!trunc || trunc > end) trunc = end;
	sid_name_ = std::string(sub, trunc - sub);
	if (sid_name_ == "DCT") sid_name_.clear();

	next(&fp, &end);

	confirm(end - fp >= 8);
	destination_ = std::string(fp, 4);

	next(&fp, &end);

	sub = strstr(fp, "DOF/");
	confirm(sub, "missing DOF in other info");
	sub += 4;
	confirm(strlen(sub) >= 6);
	std::tm tm{};
	tm.tm_year = std::stoi(std::string(sub + 0, 2)) + 100;
	tm.tm_mon  = std::stoi(std::string(sub + 2, 2)) - 1;
	tm.tm_mday = std::stoi(std::string(sub + 4, 2));
	mktime(&tm);
	dof_eobt_.date.emplace(tm.tm_wday);
}

const char *IcaoFlightPlan::callsign() {
	return callsign_.c_str();
}

api::DateTime IcaoFlightPlan::dof_eobt() {
	return dof_eobt_;
}

bool IcaoFlightPlan::is_ifr() {
	return ifr_;
}

const char *IcaoFlightPlan::departure() {
	return departure_.c_str();
}

const char *IcaoFlightPlan::destination() {
	return destination_.c_str();
}

int IcaoFlightPlan::cruise_level() {
	return cruise_level_;
}

const char *IcaoFlightPlan::route() {
	return route_.c_str();
}

std::vector<std::string> IcaoFlightPlan::points() {
	const char *route = route_.c_str(), *next;
	std::string point;
	std::vector<std::string> ret;

	do {
		next = strchr(route, ' ');

		if (next)
			point = std::string(route, next - route);
		else
			point = std::string(route);

		ret.push_back(std::move(point));
	} while (next && (route = next + strspn(next, " ")));

	return ret;
}

const char *IcaoFlightPlan::sid_name() {
	return sid_name_.c_str();
}

char IcaoFlightPlan::engine_type() {
	return aircraft->et;
}

char IcaoFlightPlan::aircraft_type() {
	return aircraft->at;
}
#endif // ifndef VFPC_STANDALONE
