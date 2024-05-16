#pragma once

#include <string>

#include "flightplan.hpp"
#include "source.hpp"

enum class Result {
	Success = 0,
	Warning,
	NonIfr,

	Pending,
	Error,
	Unknown,

	Syntax,
	SidUnknown,
	CondBan,
	CondFail,
	Destination,
	ExitPoint,
	LevelBlock,
	LevelParity,
	LevelSeries,
	Route,
	CstrBan
};

class Checker {
private:
	Source &source;

public:
	Checker(Source &);

	Result check(FlightPlan &, std::string * = nullptr);
};

class Check {
	friend class Checker;

private:
	Source &source;
	std::string log;
	FlightPlan &fp;

	Check(FlightPlan &, Source &);

	Result check();

	Result check_constraints(const api::Sid &, const char *, const char *, const std::vector<std::string> &, const std::vector<std::string> &);
	Result check_destination(const api::Constraint &, const char *);
	Result check_exit_point (const api::Constraint &, const std::vector<std::string> &);
	Result check_min_max    (const api::Constraint &, int);
	Result check_direction  (const api::Constraint &, int);
	Result check_route      (const api::Constraint &, const std::vector<std::string> &, const char *);
	Result check_alerts     (const api::Constraint &);

	Result check_restrictions(const std::vector<api::Restriction> &, const char *);
	Result check_restrictions(const std::vector<api::Restriction> &, const char *, Result &);
	void log_alternatives(const std::vector<api::Restriction> &);
};
