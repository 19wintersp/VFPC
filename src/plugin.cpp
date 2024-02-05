#include <algorithm>
#include <cstring>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <config.h>
#include "plugin.hpp"

const int TAG_ITEM_CHECK = 1;
const int TAG_ITEM_CHECK_SHORT = 2;

const int TAG_FUNC_CHECK_MENU = 100;
const int TAG_FUNC_CHECK_SHOW = 101;
const int TAG_FUNC_CHECK_TOGGLE = 102;

#define COMMAND_PREFIX ".vfpc"

void Plugin::display_message(const char *from, const char *msg, bool urgent) {
	DisplayUserMessage(PLUGIN_NAME, from, msg, true, true, urgent, urgent, false);
}

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

	RegisterTagItemType("Flight plan validity", TAG_ITEM_CHECK);
	RegisterTagItemType("Abbreviated flight plan validity", TAG_ITEM_CHECK_SHORT);
	RegisterTagItemFunction("Open options menu", TAG_FUNC_CHECK_MENU);

	display_message("", PLUGIN_NAME " version " PLUGIN_VERSION " loaded");

	logger->debug("plugin initialised");
}

Plugin::~Plugin(void) {
	logger->info("plugin unloaded");
}

const char *next_token(const char *cursor, char token[256]) {
	const char *delim = std::strchr(cursor, ' ');

	size_t count = 255;
	if (delim) count = std::min(count, (size_t)(delim - cursor));

	token[count] = 0;
	std::strncpy(token, cursor, count);

	const char *next = delim ? delim + std::strspn(delim, " ") : "";
	return *next ? next : 0;
}

bool Plugin::OnCompileCommand(const char *command) {
	char token[256];

	command = next_token(command, token);
	if (strcmp(token, COMMAND_PREFIX)) return false;

	if (!command || (command = next_token(command, token), !strcmp(token, "help"))) {
		display_message("", "Available commands:");
		display_message("", "  " COMMAND_PREFIX " help          - Display this help text");
		display_message("", "  " COMMAND_PREFIX " check [CS]... - Check the selected flight plan or specified callsign(s)");
		display_message("", "  " COMMAND_PREFIX " source [URL]  - Re/set the data server address (or local data file path)");
		display_message("", "  " COMMAND_PREFIX " reload        - Invalidate the airport data and version caches");
		display_message("", "  " COMMAND_PREFIX " debug         - Set the log level to TRACE");
		display_message("", "See <" PLUGIN_WEB "> for more information.");

		return true;
	}

	if (!strcmp(token, "debug") && !command) {
		logger->set_level(spdlog::level::trace);
		logger->trace("log tracing enabled");
	} else if (!strcmp(token, "source")) {
		// set_source(command)
	} else if (!strcmp(token, "reload") && !command) {
		// invalidate()
	} else if (!strcmp(token, "check")) {
		if (!command) {
			// check(GetSelectedFlightPlan())
		} else do {
			command = next_token(command, token);
			// check(GetFlightPlanByCallsign(token))
		} while (command);
	} else {
		display_message("", "Invalid command; run '" COMMAND_PREFIX " help' for help");
	}

	return true;
}

void Plugin::OnGetTagItem(EuroScope::CFlightPlan, EuroScope::CRadarTarget, int, int, char[16], int *, COLORREF *, double *) {}

void Plugin::OnFunctionCall(int, const char *, POINT, RECT) {}

void Plugin::OnTimer(int) {}
