#include <memory>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <config.h>
#include "plugin.hpp"

Plugin::Plugin(void) :
	EuroScope::CPlugIn(
		EuroScope::COMPATIBILITY_CODE,
		PLUGIN_NAME, PLUGIN_VERSION, PLUGIN_AUTHORS, PLUGIN_LICENCE
	),
	logger(spdlog::basic_logger_mt(PLUGIN_NAME, PLUGIN_NAME ".log"))
{
#ifdef NDEBUG
	logger->set_level(spdlog::level::trace);
#else
	logger->set_level(spdlog::level::info);
#endif

	logger->info("plugin loaded");

	DisplayUserMessage(
		PLUGIN_NAME, "Plugin", "Hello, EuroScope!",
		true, true, true, true, false
	);

	logger->debug("plugin initialised");
}

Plugin::~Plugin(void) {
	logger->info("plugin unloaded");
}
