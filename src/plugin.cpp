#include <config.h>

#include "plugin.hpp"

Plugin::Plugin(void) : EuroScope::CPlugIn(
	EuroScope::COMPATIBILITY_CODE,
	PLUGIN_NAME, PLUGIN_VERSION, PLUGIN_AUTHORS, PLUGIN_LICENCE
) {
	DisplayUserMessage(
		PLUGIN_NAME, "Plugin", "Hello, EuroScope!",
		true, true, true, true, false
	);
}

Plugin::~Plugin(void) {}
