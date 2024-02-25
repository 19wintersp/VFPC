#pragma once

#include <string>

#include <EuroScopePlugIn.hpp>

#include "source.hpp"

class Checker {
public:
	enum Result {
		Success,
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

private:
	using FlightPlan = EuroScopePlugIn::CFlightPlan;
	using FlightPlanData = EuroScopePlugIn::CFlightPlanData;

	Source &source;
	std::string *log;

	Result check_main(FlightPlan &);

	Result check_constraints(const api::Sid &, FlightPlanData &, const char *, const char *, const std::vector<std::string> &, const std::vector<std::string> &);
	Result check_destination(const api::Constraint &, const char *);
	Result check_exit_point (const api::Constraint &, const std::vector<std::string> &);
	Result check_min_max    (const api::Constraint &, int);
	Result check_direction  (const api::Constraint &, int);
	Result check_route      (const api::Constraint &, const std::vector<std::string> &, const char *);
	Result check_alerts     (const api::Constraint &);

	Result check_restrictions(const std::vector<api::Restriction> &, FlightPlanData &, const char *);
	Result check_restrictions(const std::vector<api::Restriction> &, FlightPlanData &, const char *, Result &);
	void log_alternatives(const std::vector<api::Restriction> &);

public:
	Checker(Source &);

	Result check(FlightPlan &, std::string * = nullptr);
};
