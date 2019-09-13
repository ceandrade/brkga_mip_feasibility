/**
 * @file cpxapp.cpp
 * @brief Basic Cplex Application
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * Copyright 2011 Domenico Salvagnin
 */

#include "cpxapp.h"
#include <utils/path.h>
#include <boost/lexical_cast.hpp>

namespace dominiqs
{

static const bool PRESOLVE_DEF = false;
static const double TIME_LIMIT_DEF = 10000.0;
static const double LINEAR_EPS_DEF = 1e-6;
static const int NUM_THREADS_DEF = 0;

CPXApp::CPXApp()
	: env(0), lp(0),
	presolve(PRESOLVE_DEF), timeLimit(TIME_LIMIT_DEF),
	linearEPS(LINEAR_EPS_DEF), numThreads(NUM_THREADS_DEF), preOffset(0.0)
{
	addExtension(".gz");
	addExtension(".bz2");
	addExtension(".mps");
	addExtension(".lp");
}

void CPXApp::readConfig()
{
	probName = getProbName(args.input[0], extensions);
	runName = gConfig().get("Globals", "runName", std::string("cpx"));
	presolve = gConfig().get("Globals", "presolve", PRESOLVE_DEF);
	timeLimit = gConfig().get("Globals", "timeLimit", TIME_LIMIT_DEF);
	linearEPS = gConfig().get("Globals", "eps", LINEAR_EPS_DEF);
	numThreads = gConfig().get("Globals", "numThreads", NUM_THREADS_DEF);
}

void CPXApp::startup()
{
	// init env/lp and read problem
	INIT_ENV( env );
	CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_THREADS, numThreads );
	CHECKED_CPX_CALL( CPXsetdblparam, env, CPX_PARAM_TILIM, timeLimit );
	CHECKED_CPX_CALL( CPXsetterminate, env, &UserBreak );
	INIT_PROB( env, lp );
	CHECKED_CPX_CALL( CPXreadcopyprob, env, lp, args.input[0].c_str(), 0 );
	// presolve
	std::string preName = outputDir + "/presolved.mps.gz";
	preOffset = 0.0;
	CpxPresolver presolver;
	if (presolve)
	{
		presolver.exec(env, lp);
		if (!presolver.presolvedLp) throw std::runtime_error("Empty problem after presolve!");
		preOffset = presolver.objOffset;
		CHECKED_CPX_CALL( CPXwriteprob, env, presolver.presolvedLp, preName.c_str(), 0 );
		FREE_PROB( env, presolver.presolvedLp );
		CHECKED_CPX_CALL( CPXreadcopyprob, env, lp, preName.c_str(), 0 );
	}
	else
	{
		CHECKED_CPX_CALL( CPXwriteprob, env, lp, preName.c_str(), 0 );
	}
}

void CPXApp::shutdown()
{
	FREE_PROB( env, lp );
	FREE_ENV( env );
}

} // namespace dominiqs
