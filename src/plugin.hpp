#pragma once

#include <windows.h>
#include <EuroScopePlugIn.hpp>

namespace EuroScope = EuroScopePlugIn;

class Plugin : public EuroScope::CPlugIn {
public:
	Plugin(void);
	~Plugin(void);
};
