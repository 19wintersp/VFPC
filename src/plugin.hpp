#pragma once

#include <memory>

#include <windows.h>
#include <EuroScopePlugIn.hpp>
#include <spdlog/logger.h>

namespace EuroScope = EuroScopePlugIn;

class Plugin : public EuroScope::CPlugIn {
private:
	std::shared_ptr<spdlog::logger> logger;

	void display_message(const char *, const char *, bool = false);

public:
	Plugin(void);
	~Plugin(void);

	bool OnCompileCommand(const char *) override;
	void OnGetTagItem(EuroScope::CFlightPlan, EuroScope::CRadarTarget, int, int, char[16], int *, COLORREF *, double *) override;
	void OnFunctionCall(int, const char *, POINT, RECT) override;
	void OnTimer(int) override;
};
