/**
 * @file main.cpp
 * @brief Main App
 *
 * @author Domenico Salvagnin <dominiqs at gmail dot com>
 * 2008
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/erase.hpp>

#include <utils/args_parser.h>
#include <utils/xmlconfig.h>
#include <utils/config.h>
#include <utils/path.h>
#include <utils/floats.h>
#include <utils/chrono.h>
#include <utils/randgen.h>
#include <utils/logger.h>

#include "feaspump.h"

using namespace boost;
using namespace dominiqs;

// macro type savers

#define LOG_START_SECTION(name) gLog().startSection(name)
#define LOG_END_SECTION() gLog().endSection()
#define LOG_ITEM(name, var) gLog().logItem(name, var)
#define LOG_MSG(msg) gLog().logMsg(msg)
#define LOG_CONFIG( what ) LOG_ITEM(#what, what)

struct MyData
{
public:
	MyData() : hasPresolve(false), foundSolution(false), offset(0.0), objSense(1) {}
	bool hasPresolve;
	bool foundSolution;
	double offset;
	int objSense;
	double firstValue;
	double firstTime;
	int firstIt;
	double lastValue;
	double lastTime;
	int lastIt;
};

struct SlotIncumbent
{
public:
	SlotIncumbent(MyData* md) : data(md) {}
	void operator()(double val, double t, int it) const
	{
		if (data->foundSolution)
		{
			data->lastValue = (data->objSense) * (val + data->offset);
			data->lastTime = t;
			data->lastIt = it;
		}
		else
		{
			data->firstValue = (data->objSense) * (val + data->offset);
			data->firstTime = t;
			data->firstIt = it;
			data->lastValue = (data->objSense) * (val + data->offset);
			data->lastTime = t;
			data->lastIt = it;
			data->foundSolution = true;
		}
	};
protected:
	MyData* data;
};

Prob presolve(Env env, Prob lp, MyData& data, const std::string& path)
{
	int objSense = CPXgetobjsen(env, lp);
	CHECKED_CPX_CALL( CPXpresolve, env, lp, CPX_ALG_NONE );
	int preStat;
	CHECKED_CPX_CALL( CPXgetprestat, env, lp, &preStat, 0, 0, 0, 0 );
	data.hasPresolve = false;
	if (preStat == 2) // reduced to empty problem
	{
		LOG_START_SECTION("presolvedProblem");
		LOG_ITEM("hasPresolve", "1");
		LOG_ITEM("emptyProblem", "1");
		LOG_END_SECTION();
		LOG_MSG("Too simple: presolved reduced to empty problem!");
		FREE_PROB( env, lp );
		return 0;
	}
	else if (preStat == 0) // no presolve
	{
		data.objSense = 1;
		LOG_START_SECTION("presolvedProblem");
		LOG_ITEM("hasPresolve", "0");
		LOG_ITEM("emptyProblem", "0");
		LOG_ITEM("offset", "0");
		LOG_END_SECTION();
	}
	else // get presolved problem
	{
		data.objSense = objSense;
		data.hasPresolve = true;
		// output offset
		CProb redlp;
		CHECKED_CPX_CALL( CPXgetredlp, env, lp, &redlp );
		CHECKED_CPX_CALL( CPXgetobjoffset, env, redlp, &(data.offset) );
		LOG_START_SECTION("presolvedProblem");
		LOG_ITEM("hasPresolve", "1");
		LOG_ITEM("emptyProblem", "0");
		LOG_ITEM("offset", lexical_cast<std::string>(data.offset));
		LOG_END_SECTION();
	}
	std::string preName = path + "/" + "presolved.mps.gz";
	if (data.hasPresolve)
	{
		CProb redlp;
		CHECKED_CPX_CALL( CPXgetredlp, env, lp, &redlp );
		CHECKED_CPX_CALL( CPXwriteprob, env, redlp, preName.c_str(), 0 );
	}
	else { CHECKED_CPX_CALL( CPXwriteprob, env, lp, preName.c_str(), 0 ); }
	DECL_PROB( env, redlp );
	CHECKED_CPX_CALL( CPXreadcopyprob, env, redlp, preName.c_str(), 0 );
	if (data.hasPresolve)
	{
		LOG_START_SECTION("presolvedProblem");
		LOG_ITEM("nvars", CPXgetnumcols(env, redlp));
		LOG_ITEM("binaries", CPXgetnumbin(env, redlp));
		LOG_ITEM("integers", CPXgetnumint(env, redlp));
		LOG_ITEM("nrows", CPXgetnumrows(env, redlp));
		LOG_END_SECTION();
	}
	CPXsetdefaults(env);
	return redlp;
}

static const char* FP_VERSION = "2.1";
static const uint64_t DEF_SEED = 120507;

int main (int argc, char const *argv[])
{
	// config/options
	ArgsParser args;
	args.parse(argc, argv);
	if (args.input.size() < 1)
	{
		std::cout << "usage: feaspump prob_file" << std::endl;
		return -1;
	}
	std::map<std::string, std::string> shortcuts;
	shortcuts["g"] = "Globals";
	shortcuts["fp"] = "FeasibilityPump";
	mergeConfig(args, gConfig(), shortcuts);
	std::string runName = gConfig().get("Globals", "runName", std::string("default"));
	bool mipPresolve = gConfig().get("Globals", "mipPresolve", true );
	int numThreads = gConfig().get("Globals", "numThreads", 4 );
	bool printSol = gConfig().get("Globals", "printSol", true );

	// seed
	uint64_t seed = gConfig().get<uint64_t>("Globals", "seed", DEF_SEED);
	seed = generateSeed(seed);
	gConfig().set<uint64_t>("Globals", "seed", seed);
	std::string probName = Path(args.input[0]).getBasename();
	ierase_last(probName, ".gz");
	ierase_last(probName, ".mps");
	ierase_last(probName, ".lp");
	std::string defOutDir = "./tmp/" + probName;
	std::string outputDir = gConfig().get("Globals", "outputDir", defOutDir);

	std::stringstream ss;
	ss << "./results/tmp/run_" << seed << "/" << probName;
	outputDir = ss.str();

	std::cout << "Output dir: " << outputDir << std::endl;
	// logger
	gLog().open("run.xml", outputDir);
	gLog().setConsoleEcho(true);
	LOG_START_SECTION("config");
	LOG_ITEM("probName", probName);
	LOG_ITEM("runName", runName);
	LOG_ITEM("presolve", mipPresolve);
	LOG_ITEM("numThreads", numThreads);
	LOG_ITEM("cpxVersion", CPX_VERSION);
	LOG_ITEM("gitHash", GIT_HASH);
	LOG_ITEM("fpVersion", FP_VERSION);
	LOG_END_SECTION();

	DECL_ENV( env );
	double integralityEps = 1e-6;
	CHECKED_CPX_CALL( CPXgetdblparam, env, CPX_PARAM_EPINT, &integralityEps );
	gConfig().set("FeasibilityPump", "integralityEps", integralityEps);
	DECL_PROB( env, origLp );
	Prob lp = 0;

	std::stringstream output_carlos;
	try
	{
		CHECKED_CPX_CALL( CPXreadcopyprob, env, origLp, args.input[0].c_str(), 0 );
		LOG_START_SECTION("originalProblem");
		LOG_ITEM("nvars", CPXgetnumcols(env, origLp));
		LOG_ITEM("binaries", CPXgetnumbin(env, origLp));
		LOG_ITEM("integers", CPXgetnumint(env, origLp));
		LOG_ITEM("nrows", CPXgetnumrows(env, origLp));
		LOG_END_SECTION();
		// presolve
		MyData data;
		if (mipPresolve)
		{
			lp = presolve(env, origLp, data, outputDir);
			if (!lp) throw std::runtime_error("Problem to easy (solved in presolve stage)");
		}
		else lp = origLp;
		// feaspump
		CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_THREADS, numThreads );
		FeasibilityPump solver;
		solver.readConfig();
		solver.emitNewIncumbent = SlotIncumbent(&data);
		if (numThreads != 1) gChrono().setDefaultType(Chrono::WALL_CLOCK);
		gChrono().start();
		solver.init(env, lp);
		solver.pump();
		solver.reset();
		gChrono().stop();
		if (data.foundSolution)
		{
			LOG_START_SECTION("results");
			LOG_ITEM("firstValue", data.firstValue);
			LOG_ITEM("firstIt", data.firstIt);
			LOG_ITEM("firstTime", data.firstTime);
			LOG_ITEM("lastValue", data.lastValue);
			LOG_ITEM("lastIt", data.lastIt);
			LOG_ITEM("lastTime", data.lastTime);
			LOG_END_SECTION();
			// print solution
			std::ofstream out((outputDir + "/solution.sol").c_str());
			std::vector<double> preX;
			std::vector<double> x(CPXgetnumcols(env, origLp));
			solver.getSolution(preX);
			if (data.hasPresolve)
			{
				DOMINIQS_ASSERT( (int)preX.size() == CPXgetnumcols(env, lp) );
				CHECKED_CPX_CALL( CPXuncrushx, env, origLp, &x[0], &preX[0] );
				int n = CPXgetnumcols(env, origLp);
				std::vector<double> obj(n);
				CHECKED_CPX_CALL( CPXgetobj, env, origLp, &obj[0], 0, n - 1 );
				double objValue = dotProduct(&obj[0], &x[0], n);
				DOMINIQS_ASSERT( relEqual(objValue, data.lastValue) );
			}
			else x = preX;
			out << "=obj= " << std::setprecision(15) << data.lastValue << std::endl;
			if (printSol) std::cout << "Solution: =obj= " << data.lastValue << std::endl;
			std::vector<std::string> xNames;
			getVarNames(env, origLp, xNames);
			DOMINIQS_ASSERT( xNames.size() == x.size() );
			for (unsigned int i = 0; i < x.size(); i++)
			{
				if (printSol && isNotNull(x[i], integralityEps)) std::cout << xNames[i] << " = " << x[i] << std::endl;
				out << xNames[i] << " " << std::setprecision(15) << x[i] << std::endl;
			}
		}
		else
		{
			LOG_START_SECTION("results");
			LOG_MSG("No solution found!");
			LOG_END_SECTION();
		}

		//---------------
		// Carlos' logs
		//---------------
		output_carlos
		    << "\n\nInstance & Seed & Threads & MaxTime & Iters & LUTWall & "
            << "FinalStage & Viability & Value\n"
		    << probName << " & "
		    << seed << " & "
		    << numThreads << " & "
		    << gConfig().get("FeasibilityPump", "timeLimit", 3600) << " & "
		    << data.lastIt << " & "
		    << std::setiosflags(std::ios::fixed) << std::setprecision(2)
		    << solver.totalTime << " & "
		    << solver.finalStage << " & "
		    << (data.foundSolution? "feasible" : "infeasible") << " & "
		    << std::setprecision(6)
		    << data.lastValue;
	}
	catch(std::exception& e)
	{
		std::cout << "Exception: " << e.what() << std::endl;
		gLog().logMsg(e.what());
	}
	FREE_PROB( env, lp );
	FREE_ENV( env );
	gLog().close();

	std::cout << output_carlos.str();
	std::cout.flush();

	return 0;
}
