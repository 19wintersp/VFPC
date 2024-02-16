#include <chrono>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/daily_file_sink.h>

#include <config.h>
#include "plugin.hpp"

Plugin *plugin;

void __declspec(dllexport) EuroScopePlugInInit(EuroScope::CPlugIn **ptr) {
	auto logger = spdlog::daily_logger_mt("log", PLUGIN_NAME ".log", 0, 0, false, 10);
	spdlog::set_default_logger(logger);
	spdlog::flush_every(std::chrono::seconds(15));

#ifdef NDEBUG
	spdlog::set_level(spdlog::level::trace);
#else
	spdlog::set_level(spdlog::level::info);
#endif

	*ptr = plugin = new Plugin;
}

void __declspec(dllexport) EuroScopePlugInExit(void) {
	delete plugin;
}
