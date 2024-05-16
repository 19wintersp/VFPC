#pragma once

#include <string>
#include <vector>

#ifndef VFPC_STANDALONE
#include <windows.h>
#include <EuroScopePlugIn.hpp>
#endif

#ifdef VFPC_STANDALONE
#include <icao-aircraft.hpp>
#include "source.hpp"
#endif

class FlightPlan {
public:
	virtual bool is_ifr() = 0;

	virtual const char *departure() = 0;
	virtual const char *destination() = 0;

	virtual int cruise_level() = 0;
	virtual const char *route() = 0;
	virtual std::vector<std::string> points() = 0;
	virtual const char *sid_name() = 0;

	virtual char engine_type() = 0;
	virtual char aircraft_type() = 0;
};

#ifndef VFPC_STANDALONE
class EuroScopeFlightPlan : public virtual FlightPlan {
private:
	using FlightPlan = EuroScopePlugIn::CFlightPlan;
	using FlightPlanData = EuroScopePlugIn::CFlightPlanData;

	const FlightPlan &fp;

public:
	EuroScopeFlightPlan(const FlightPlan &);

	bool is_ifr() override;

	const char *departure() override;
	const char *destination() override;

	int cruise_level() override;
	const char *route() override;
	std::vector<std::string> points() override;
	const char *sid_name() override;

	char engine_type() override;
	char aircraft_type() override;
};
#endif // ifndef VFPC_STANDALONE

#ifdef VFPC_STANDALONE
class IcaoFlightPlan : public virtual FlightPlan {
private:
	bool ifr_;
	int cruise_level_;
	std::string callsign_, departure_, destination_, route_, sid_name_;
	api::DateTime dof_eobt_;

	IcaoAircraft *aircraft;

public:
	IcaoFlightPlan(const char *);

	const char *callsign();
	api::DateTime dof_eobt();

	bool is_ifr() override;

	const char *departure() override;
	const char *destination() override;

	int cruise_level() override;
	const char *route() override;
	std::vector<std::string> points() override;
	const char *sid_name() override;

	char engine_type() override;
	char aircraft_type() override;
};
#endif // ifdef VFPC_STANDALONE
