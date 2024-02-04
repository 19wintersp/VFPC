#pragma once

#include <memory>

#include <windows.h>
#include <EuroScopePlugIn.hpp>
#include <spdlog/logger.h>

namespace EuroScope = EuroScopePlugIn;

class Plugin : public EuroScope::CPlugIn {
private:
	std::shared_ptr<spdlog::logger> logger;

public:
	Plugin(void);
	~Plugin(void);
};
