#include <algorithm>
#include <cstring>
#include <exception>

#include <spdlog/spdlog.h>

#include <config.h>
#include "plugin.hpp"

const int TAG_ITEM_CHECK = 1;
const int TAG_ITEM_CHECK_SHORT = 2;

const int TAG_FUNC_CHECK_MENU = 100;
const int TAG_FUNC_CHECK_SHOW = 101;
const int TAG_FUNC_CHECK_TOGGLE = 102;

const COLORREF TAG_COLOUR_GREY = 0xbebebe;
const COLORREF TAG_COLOUR_PASS = 0x00be00;
const COLORREF TAG_COLOUR_WARN = 0x0079f1;
const COLORREF TAG_COLOUR_FAIL = 0x0000be;

const int UPDATE_INTERVAL = 30;

#define COMMAND_PREFIX ".vfpc"

void log_caught_exception(const char *ctx);

void Plugin::display_message(const char *from, const char *msg, bool urgent) {
	DisplayUserMessage(PLUGIN_NAME, from, msg, true, true, urgent, urgent, false);
}

Plugin::Plugin(void) :
	EuroScope::CPlugIn(
		EuroScope::COMPATIBILITY_CODE,
		PLUGIN_NAME, PLUGIN_VERSION, PLUGIN_AUTHORS, PLUGIN_LICENCE
	),
	checker(source)
{
	spdlog::info("plugin loaded");

	RegisterTagItemType("Flight plan validity", TAG_ITEM_CHECK);
	RegisterTagItemType("Abbreviated flight plan validity", TAG_ITEM_CHECK_SHORT);
	RegisterTagItemFunction("Open options menu", TAG_FUNC_CHECK_MENU);

	display_message("", PLUGIN_NAME " version " PLUGIN_VERSION " loaded");

	spdlog::debug("plugin initialised");
}

Plugin::~Plugin() {
	spdlog::info("plugin unloaded");
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
		display_message("", "  " COMMAND_PREFIX " check [CS]... - Check the selected or specified flight plan(s)");
		display_message("", "  " COMMAND_PREFIX " source [URL]  - Re/set the data server address");
		display_message("", "  " COMMAND_PREFIX " source <FILE> - Load airport data from a local file into the cache");
		display_message("", "  " COMMAND_PREFIX " reload        - Invalidate the airport data and version caches");
		display_message("", "  " COMMAND_PREFIX " debug         - Set the log level to TRACE");
		display_message("", "See <" PLUGIN_WEB "> for more information.");

		return true;
	}

	try {
		if (!strcmp(token, "debug") && !command) {
			spdlog::set_level(spdlog::level::trace);
			spdlog::trace("log tracing enabled");
		} else if (!strcmp(token, "source")) {
			source.set(command);
		} else if (!strcmp(token, "reload") && !command) {
			source.invalidate();
			source.update();
		} else if (!strcmp(token, "check")) {
			std::string log;

			if (!command) {
				auto fp = FlightPlanSelectASEL();
				if (fp.IsValid()) {
					spdlog::trace("manual check (by selection) for {}", fp.GetCallsign());

					checker.check(fp, &log);
					display_message(fp.GetCallsign(), log.c_str(), true);
				} else {
					display_message("", "No flight plan selected", true);
				}
			} else do {
				command = next_token(command, token);

				auto fp = FlightPlanSelect(token);
				if (fp.IsValid()) {
					spdlog::trace("manual check (by callsign) for {}", fp.GetCallsign());

					checker.check(fp, &log);
					display_message(fp.GetCallsign(), log.c_str(), true);
				}
			} while (command);
		} else {
			display_message("", "Invalid command; run '" COMMAND_PREFIX " help' for help", true);
		}
	} catch (...) {
		Plugin::report_exception("user command execution");
	}

	return true;
}

void Plugin::OnGetTagItem(
	EuroScope::CFlightPlan flight_plan,
	EuroScope::CRadarTarget _radar_target,
	int item_code,
	int _tag_data,
	char item_value[16],
	int *item_color,
	COLORREF *item_rgb,
	double *_item_font_size
) {
	if (item_code == TAG_ITEM_CHECK || item_code == TAG_ITEM_CHECK_SHORT) {
		try {
			if (!flight_plan.IsValid()) return;

			Checker::Result result = checker.check(flight_plan);

			*item_color = EuroScope::TAG_COLOR_RGB_DEFINED;

			switch (result) {
				case Checker::Result::Pending:
					*item_rgb = TAG_COLOUR_GREY;
					break;
				
				case Checker::Result::Success:
					*item_rgb = TAG_COLOUR_PASS;
					break;

				case Checker::Result::Error:
				case Checker::Result::Unknown:
				case Checker::Result::Warning:
				case Checker::Result::NonIfr:
					*item_rgb = TAG_COLOUR_WARN;
					break;

				default:
					*item_rgb = TAG_COLOUR_FAIL;
					break;
			}

			bool abbr = item_code == TAG_ITEM_CHECK_SHORT;
			const char *value = "*";

			#define CASE(result, short, long) \
				case Checker::Result::result: value = abbr ? short : long; break;

			switch (result) {
				CASE(Success,     "/", "OK!");
				CASE(Warning,     "?", "OK?");
				CASE(NonIfr,      "?", "VFR");
				CASE(Pending,     ".", "...");
				CASE(Error,       ".", "...");
				CASE(Unknown,     "?", "DAT");
				CASE(Syntax,      "!", "RTE");
				CASE(SidUnknown,  "!", "SID");
				CASE(CondBan,     "!", "BAN");
				CASE(CondFail,    "!", "RST");
				CASE(Destination, "!", "DST");
				CASE(ExitPoint,   "!", "EPT");
				CASE(LevelBlock,  "!", "LVL");
				CASE(LevelParity, "!", "OER");
				CASE(LevelSeries, "!", "LVL");
				CASE(Route,       "!", "RTE");
				CASE(CstrBan,     "!", "BAN");
			}

			#undef CASE

			strcpy(item_value, value);
		} catch (...) {
			Plugin::report_exception("flight plan check");
		}
	}
}

void Plugin::OnFunctionCall(int, const char *, POINT, RECT) {}

void Plugin::OnTimer(int time) {
	if (last_update < 0 || (time - last_update) > UPDATE_INTERVAL) {
		source.update();
		last_update = time;
	}

	// report accumulated errors, but don't bother waiting for the lock if held
	if (errors_lock.try_lock()) {
		std::vector<std::string> errors_taken;
		std::swap(errors_taken, errors);

		errors_lock.unlock();

		for (std::string &ctx : errors_taken) {
			ctx.insert(0, "Error in ");
			ctx.append(" (check log for details)");

			display_message("", ctx.c_str(), true);
		}
	}
}

void Plugin::report_exception(const char *ctx) {
	const char *err = "(unknown)";

	try {
		throw;
	} catch (const std::exception &ex) {
		err = ex.what();
	} catch (const std::string &ex) {
		err = ex.c_str();
	} catch (...) {}

	if (ctx) spdlog::warn("caught exception in {}: {}", ctx, err);
	else spdlog::warn("caught exception: {}", err);

	std::lock_guard<std::mutex> _lock(errors_lock);

	errors.push_back(std::string(ctx ? ctx : ""));
}
