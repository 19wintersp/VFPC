#ifndef VFPC_STANDALONE
#error Cannot compile test program in default plugin mode!
#endif

#include <fstream>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <config.h>
#include "check.hpp"
#include "flightplan.hpp"
#include "source.hpp"

int main(int argc, const char *argv[]) {
	const char *argv0 = argc ? argv[0] : "vfpc";

	if (argc != 2) {
		std::cerr
			<< "Usage: " << argv0 << " <FILE>\n"
			<< "Validate ICAO flight plan on stdin against rules in FILE.\n\n"

			<< "Exit status:\n"
			<< "  0        flight plan validated successfully\n"
			<< "  1-100    flight plan failed validation\n"
			<< "  101      error unrelated to flight plan validation\n\n"

			<< PLUGIN_NAME " (test) v" PLUGIN_VERSION " by " PLUGIN_AUTHORS "\n"
			<< "Licensed under the " PLUGIN_LICENCE ".\n"
			<< "See <" PLUGIN_WEB "> for more information.\n";

		return 101;
	}

	try {
		std::string line, fp_src;
		while (std::getline(std::cin, line)) fp_src.append(line);

		IcaoFlightPlan fp(fp_src.c_str());

		std::ifstream file(argv[1]);
		nlohmann::json data = nlohmann::json::parse(file);

		StaticSource source(data, fp.dof_eobt());
		Checker checker(source);

		std::string log;
		Result result = checker.check(fp, &log);

		std::cerr << "Result for " << fp.callsign() << ": " << log << "\n";

		return (int) result;
	} catch (...) {
		std::cerr << "Exception thrown\n";

		return 101;
	}
}
