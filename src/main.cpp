#include <list>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <unordered_set>
#include <unordered_map>
#include <cstring>
#include <fstream>
#include <queue>
#include <sstream>

#include "global.h"
#include "db/database.h"
#include "route/aStarRoute.h"

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>

#include <cxxopts.hpp>

using namespace std;

int main(int argc, char **argv) {
	cxxopts::Options options("Potter", "An Open-Source High-Concurrency and High-Performance Parallel Router for UltraScale FPGAs");
	options.add_options()
		("h,help", "Print help information")
		("i,input", "[REQUIRED] The input (unrouted) physical netlist", cxxopts::value<std::string>())
		("o,output", "[REQUIRED] The output (routed) physical netlist", cxxopts::value<std::string>())
		("d,device", "The device file", cxxopts::value<std::string>()->default_value("xcvu3p.device"))
		("t,thread", "The number of threads", cxxopts::value<int>()->default_value("32"))
		("r,runtime_first", "Enable runtime first mode", cxxopts::value<bool>()->implicit_value("true")->default_value("false"));

	auto result = options.parse(argc, argv);

	if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

	if (!result.count("input") || !result.count("output")) {
		std::cerr << "Input and output files must be specified!!!!!" << endl;
		std::cerr << options.help() << std::endl;
		return 1;
	}

	string inputName = result["input"].as<std::string>();
	string outputName = result["output"].as<std::string>();
	string deviceName = result["device"].as<std::string>();
	int numThread = result["thread"].as<int>();
	bool isRuntimeFirst = result["runtime_first"].as<bool>();

	log() << "input: " << result["input"].as<std::string>() << endl;
	log() << "output: " << result["output"].as<std::string>() << endl;
	log() << "device: " << result["device"].as<std::string>() << endl;
	log() << "thread: " << result["thread"].as<int>() << endl;
	log() << "runtime first: " << (result["runtime_first"].as<bool>() ? "true" : "false") << endl;
	log() << endl;

	Database database;	
	database.setNumThread(numThread);
	database.readDevice(deviceName); // TODO: try to load pre-computed device file
	database.readNetlist(inputName);		
	database.setRouteNodeChildren();
	database.printStatistic();

	// setting 
	database.useRW = false;

	// routing
	aStarRoute router(database, isRuntimeFirst);
	router.route();

	// write back
	database.writeNetlist(outputName, router.nodeRoutingResults);
	database.device.check_memory_peak(-1);
	return 0;
}
