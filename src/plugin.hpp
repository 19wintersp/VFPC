#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>
#include <EuroScopePlugIn.hpp>

#include "check.hpp"
#include "source.hpp"

namespace EuroScope = EuroScopePlugIn;

class Plugin : public EuroScope::CPlugIn {
private:
	Source source;
	Checker checker;
	int last_update = -1;

	inline static std::mutex errors_lock;
	inline static std::vector<std::string> errors;

	void display_message(const char *, const char *, bool = false);

public:
	Plugin(void);
	~Plugin();

	bool OnCompileCommand(const char *) override;
	void OnGetTagItem(EuroScope::CFlightPlan, EuroScope::CRadarTarget, int, int, char[16], int *, COLORREF *, double *) override;
	void OnFunctionCall(int, const char *, POINT, RECT) override;
	void OnTimer(int) override;

	static void report_exception(const char *ctx);
};
