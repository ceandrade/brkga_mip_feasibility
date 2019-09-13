/**
 * @file app.cpp
 * @brief Basic Application
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * Copyright 2011 Domenico Salvagnin
 */

#include "app.h"
#include "randgen.h"
#include "logger.h"
#include "xmlconfig.h"
#include "path.h"
#include <signal.h>

namespace dominiqs
{

int UserBreak = 0;

static void userSignalBreak(int signum)
{
	UserBreak = 1;
}

App::App() : seed(0), parseDone(false)
{
	addShortcut("g", "Globals");
}

void App::addShortcut(const std::string& shortName, const std::string& fullName)
{
	shortcuts[shortName] = fullName;
}

void App::addExtension(const std::string& ext)
{
	extensions.push_back(ext);
}

bool App::parseArgsAndConfig(int argc, char const *argv[])
{
	// parse command line arguments and check usage
	args.parse(argc, argv);
	if (!checkUsage()) return false;
	// read config
	mergeConfig(args, gConfig(), shortcuts);
	// seed generation
	seed = gConfig().get<uint64_t>("Globals", "seed", 0);
	parseDone = true;
	return true;
}

int App::run()
{
	if (!parseDone) return -1;
	int retValue = 0;

	// setup logger
	std::string probName = getProbName(args.input[0], extensions);
	std::string defOutDir = "./tmp/" + probName;
	outputDir = gConfig().get("Globals", "outputDir", defOutDir);
	gConfig().set("Globals", "outputDir", outputDir);
	std::cout << "Output dir: " << outputDir << std::endl;
	gLog().open("run.xml", outputDir);
	// read config
	readConfig();
	// Ctrl-C handling
	UserBreak = 0;
	void (*previousHandler)(int) = ::signal(SIGINT, userSignalBreak);
	try
	{
		startup();
		exec();
		shutdown();
	}
	catch(std::exception& e)
	{
		retValue = -1;
		gLog().setConsoleEcho(true);
		GlobalAutoSection closer("error");
		gLog().logMsg(e.what());
	}
	// restore signal handler
	::signal(SIGINT, previousHandler);
	// close logger
	gLog().close();
	return retValue;
}

} // namespace dominiqs
