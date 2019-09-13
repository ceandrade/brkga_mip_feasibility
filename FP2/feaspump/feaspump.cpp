/**
 * @file feaspump.cpp
 * @brief Feasibility Pump algorithm Source
 *
 * @author Domenico Salvagnin <dominiqs at gmail dot com>
 * 2008
 */

#include <map>
#include <signal.h>
#include <cmath>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <boost/assign/std/vector.hpp>
#include <boost/format.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include <utils/asserter.h>
#include <utils/floats.h>
#include <utils/maths.h>
#include <utils/logger.h>
#include <utils/xmlconfig.h>

#include "feaspump.h"

using namespace boost;
using namespace boost::assign;
using namespace dominiqs;

// macro type savers

#define MYLOG(flag, actions) do { if (flag) { actions } } while(false);
#define READ_FROM_CONFIG( what, defValue ) what = gConfig().get("FeasibilityPump", #what, defValue)
#define LOG_CONFIG( what ) gLog().logItem(#what, what)

/**
 * Ctrl-C handling
 */

int AbortOptimization = 0;

void userSignalBreak(int signum)
{
	AbortOptimization = 1;
}

static bool isSolutionInteger(const std::vector<int>& integers, const std::vector<double>& x, double eps)
{
	for (int j: integers) if (!isInteger(x[j], eps)) return false;
	return true;
}

static double solutionFractionality(const std::vector<int>& integers, const std::vector<double>& x)
{
	double tot = 0.0;
	for (int j: integers) tot += fabs(x[j] - floor(x[j] + 0.5));
	return tot;
}

static int solutionNumFractional(const std::vector<int>& integers, const std::vector<double>& x, double eps)
{
	int tot = 0;
	for (int j: integers) if (!isInteger(x[j], eps)) tot++;
	return tot;
}

static double solutionsDistance(const std::vector<int>& integers, const std::vector<double>& x1, const std::vector<double>& x2)
{
	double tot = 0.0;
	for (int j: integers) tot += fabs(x1[j] - x2[j]);
	return tot;
}

static bool areSolutionsEqual(const std::vector<int>& integers, const std::vector<double>& x1, const std::vector<double>& x2, double eps)
{
	for (int j: integers) if (different(x1[j], x2[j], eps)) return false;
	return true;
}

namespace dominiqs {

/**
 * Print vector of numbers
 */

template<class T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& v)
{
	unsigned int size = v.size();
	for (unsigned int i = 0; i < size; i++) out << v[i] << " ";
	return out;
}

static const double DEF_TIME_LIMIT = 7200.0;
static const int DEF_S1_ITER_LIMIT = 10000;
static const int DEF_ITER_LIMIT = 2000;
static const int DEF_S1_MAX_MISSED_DECREASE = 30;
static const int DEF_MAX_MISSED_DECREASE = 50;
static const int DEF_MAX_RESTARTS = 100;
static const int DEF_AVG_FLIPS = 20;
static const double DEF_INTEGRALITY_EPS = 1e-6;
static const uint64_t DEF_SEED = 120507;
static const double DEF_SIGMA_THR = 0.02;
static const bool DEF_PERTURBE_FROM_TRANSFORMER_OUTPUT = true;
static const double DEF_ALPHA = 0.0;
static const double DEF_ALPHA_FACTOR = 0.9;
static const double DEF_ALPHA_DIST = 0.005;
static const bool DEF_DO_STAGE_3 = true;
static const char DEF_FIRST_OPT_METHOD = 'S';
static const int DEF_OPT_ITER_LIMIT = -1;
static const char DEF_REOPT_METHOD = 'S';
static const int DEF_REOPT_ITER_LIMIT = -1;
static const bool DEF_LOG_PERTURBATION = false;
static const bool DEF_LOG_DETAILS = false;
static const bool DEF_LOG_ITERATION = false;
static const bool DEF_LOG_TIME = false;
static const bool DEF_LOG_LP = false;

static const int EXTENDED_PRECISION = 4;
static const double GEOM_FACTOR = 0.85;
static const double BIGM = 1e9;
static const double BIGBIGM = 1e15;

FeasibilityPump::FeasibilityPump() :
	timeLimit(DEF_TIME_LIMIT),
	s1IterLimit(DEF_S1_ITER_LIMIT), iterLimit(DEF_ITER_LIMIT),
	s1MaxMissedDecrease(DEF_S1_MAX_MISSED_DECREASE), maxMissedDecrease(DEF_MAX_MISSED_DECREASE),
	maxRestarts(DEF_MAX_RESTARTS), avgFlips(DEF_AVG_FLIPS), integralityEps(DEF_INTEGRALITY_EPS),
	seed(DEF_SEED), sigmaThr(DEF_SIGMA_THR),
	perturbeFromTransformerOutput(DEF_PERTURBE_FROM_TRANSFORMER_OUTPUT),
	alpha(DEF_ALPHA), alphaFactor(DEF_ALPHA_FACTOR), alphaDist(DEF_ALPHA_DIST),
	doStage3(DEF_DO_STAGE_3),
	firstOptMethod(DEF_FIRST_OPT_METHOD), optIterLimit(DEF_OPT_ITER_LIMIT),
	reOptMethod(DEF_REOPT_METHOD), reOptIterLimit(DEF_REOPT_ITER_LIMIT),
	logPerturbation(DEF_LOG_PERTURBATION),
	logDetails(DEF_LOG_DETAILS),
	logIteration(DEF_LOG_ITERATION),
	logTime(DEF_LOG_TIME),
	logLP(DEF_LOG_LP),
	m_env(0), m_lp(0),
	totalTime(0.0),
	finalStage(0)
{
}

void FeasibilityPump::readConfig()
{
	std::string frac2intName = gConfig().get("FeasibilityPump", "frac2int", std::string("propround"));
	frac2int = SolutionTransformerPtr(TransformersFactory::getInstance().create(frac2intName));
	DOMINIQS_ASSERT( frac2int );
	// optimization methods
	std::string firstMethod = gConfig().get("FeasibilityPump", "firstOptMethod", std::string("default"));
	if (firstMethod == "default") firstOptMethod = 'S';
	else if (firstMethod == "primal") firstOptMethod = 'P';
	else if (firstMethod == "dual") firstOptMethod = 'D';
	else if (firstMethod == "barrier") firstOptMethod = 'B';
	else throw std::runtime_error(std::string("Unknown optimization method: ") + firstMethod);
	std::string reMethod = gConfig().get("FeasibilityPump", "reOptMethod", std::string("default"));
	if (reMethod == "default") reOptMethod = 'S';
	else if (reMethod == "primal") reOptMethod = 'P';
	else if (reMethod == "dual") reOptMethod = 'D';
	else if (reMethod == "barrier") reOptMethod = 'B';
	else throw std::runtime_error(std::string("Unknown optimization method: ") + reMethod);
	// other options
	READ_FROM_CONFIG( timeLimit, DEF_TIME_LIMIT );
	READ_FROM_CONFIG( s1IterLimit, DEF_S1_ITER_LIMIT );
	READ_FROM_CONFIG( iterLimit, DEF_ITER_LIMIT );
	READ_FROM_CONFIG( s1MaxMissedDecrease, DEF_S1_MAX_MISSED_DECREASE );
	READ_FROM_CONFIG( maxMissedDecrease, DEF_MAX_MISSED_DECREASE );
	READ_FROM_CONFIG( maxRestarts, DEF_MAX_RESTARTS );
	READ_FROM_CONFIG( avgFlips, DEF_AVG_FLIPS );
	READ_FROM_CONFIG( integralityEps, DEF_INTEGRALITY_EPS );
	seed = gConfig().get<uint64_t>("Globals", "seed", DEF_SEED);
	READ_FROM_CONFIG( sigmaThr, DEF_SIGMA_THR );
	READ_FROM_CONFIG( perturbeFromTransformerOutput, DEF_PERTURBE_FROM_TRANSFORMER_OUTPUT );
	READ_FROM_CONFIG( alpha, DEF_ALPHA );
	READ_FROM_CONFIG( alphaFactor, DEF_ALPHA_FACTOR );
	READ_FROM_CONFIG( alphaDist, DEF_ALPHA_DIST );
	READ_FROM_CONFIG( doStage3, DEF_DO_STAGE_3 );
	READ_FROM_CONFIG( optIterLimit, DEF_OPT_ITER_LIMIT );
	READ_FROM_CONFIG( reOptIterLimit, DEF_REOPT_ITER_LIMIT );
	READ_FROM_CONFIG( logPerturbation, DEF_LOG_PERTURBATION );
	READ_FROM_CONFIG( logDetails, DEF_LOG_DETAILS );
	READ_FROM_CONFIG( logIteration, DEF_LOG_ITERATION );
	READ_FROM_CONFIG( logTime, DEF_LOG_TIME );
	READ_FROM_CONFIG( logLP, DEF_LOG_LP );
	// display options
	display.headerInterval = gConfig().get("Globals", "headerInterval", 10);
	display.iterationInterval = gConfig().get("Globals", "iterationInterval", 1);
	// log config
	gLog().startSection("config");
	gLog().logItem("frac2int", frac2intName);
	gLog().logItem("firstOptMethod", firstMethod);
	gLog().logItem("reOptMethod", reMethod);
	LOG_CONFIG( timeLimit );
	LOG_CONFIG( iterLimit );
	LOG_CONFIG( s1IterLimit );
	LOG_CONFIG( s1MaxMissedDecrease );
	LOG_CONFIG( maxMissedDecrease );
	LOG_CONFIG( maxRestarts );
	LOG_CONFIG( avgFlips );
	LOG_CONFIG( integralityEps );
	LOG_CONFIG( seed );
	LOG_CONFIG( sigmaThr );
	LOG_CONFIG( perturbeFromTransformerOutput );
	LOG_CONFIG( optIterLimit );
	LOG_CONFIG( reOptIterLimit );
	LOG_CONFIG( alpha );
	LOG_CONFIG( alphaFactor );
	LOG_CONFIG( alphaDist );
	LOG_CONFIG( doStage3 );
	LOG_CONFIG( logPerturbation );
	LOG_CONFIG( logDetails );
	LOG_CONFIG( logIteration );
	LOG_CONFIG( logTime );
	LOG_CONFIG( logLP );
	gLog().endSection();
	rnd.setSeed(seed);
	rnd.warmUp();
	frac2int->readConfig();
}

void FeasibilityPump::getSolution(std::vector<double>& x) const
{
	x.resize(incumbent.size());
	copy(incumbent.begin(), incumbent.end(), x.begin());
}

double FeasibilityPump::getSolutionValue(const std::vector<double>& x) const
{
	return std::inner_product(x.begin(), x.end(), obj.begin(), 0.0);
}

int FeasibilityPump::getIterations() const
{
	return nitr;
}

void FeasibilityPump::reset()
{
	nitr = 0;
	pertCnt = 0;
	restartCnt = 0;
	lastRestart = 0;
	flipsInRestart = 0;
	maxFlipsInRestart = 0;
	lastIntegerX.clear();
	chrono.reset();
	lpChrono.reset();
	roundChrono.reset();
	chrono.setDefaultType(gChrono().getDefaultType());
	lpChrono.setDefaultType(gChrono().getDefaultType());
	roundChrono.setDefaultType(gChrono().getDefaultType());
	fixed.clear();
	binaries.clear();
	gintegers.clear();
	integers.clear();
	isPureInteger = false;
	isBinary = false;
	m_lp = 0;
	m_env = 0;
}

void FeasibilityPump::init(Env env, Prob lp, const std::vector<char>& ctype)
{
	DOMINIQS_ASSERT( env );
	DOMINIQS_ASSERT( lp );
	DOMINIQS_ASSERT( frac2int );
	// INIT
	GlobalAutoSection initS("fpInit");
	reset();
	m_env = env;
	m_lp = lp;
	objsen = CPXgetobjsen(m_env, m_lp);
	// if user passed column type information, used it
	if (ctype.size()) { CHECKED_CPX_CALL( CPXcopyctype, m_env, m_lp, &ctype[0] ); }
	model.extract(m_env, m_lp);
	frac2int->init(model, true);
	int n = CPXgetnumcols(m_env, m_lp);
	frac_x.resize(n, 0);
	integer_x.resize(n, 0);
	obj.resize(n, 0);
	CHECKED_CPX_CALL( CPXgetobj, m_env, m_lp, &obj[0], 0, n - 1 );
	objNorm = sqrt(dotProduct(&obj[0], &obj[0], n));
	for (int i = 0; i < n; i++)
	{
		// fixed variables are not considered integer variables, of any kind
		// this is correct if the rounding function does not alter their value!
		// this is trivially true for simple rounding, but some care must be
		// taken for more elaborate strategies!!!
		if ( equal(model.xLb[i], model.xUb[i], integralityEps) ) fixed.push_back(i);
		else if ( model.xType[i] != 'C' )
		{
			integers.push_back(i);
			if (model.xType[i] == 'B') binaries.push_back(i);
			else gintegers.push_back(i);
		}
	}
	gLog().logItem("fixedCnt", fixed.size());
	isBinary = (gintegers.size() == 0);
	gLog().logItem( "isBinary", isBinary );
	isPureInteger = (fixed.size() + integers.size() == (unsigned int)n);
	gLog().logItem( "isPureInteger", isPureInteger );

	CHECKED_CPX_CALL( CPXchgprobtype, m_env, m_lp, CPXPROB_LP );
}

bool FeasibilityPump::pump(const std::vector<double>& xStart, bool pFeas)
{
	DOMINIQS_ASSERT( m_env );
	DOMINIQS_ASSERT( m_lp );
	DOMINIQS_ASSERT( frac2int );
	chrono.start();
	int n = model.numVars;
	primalFeas = false;

	// install signal handler
	void (*previousHandler)(int) = ::signal(SIGINT, userSignalBreak);
	CHECKED_CPX_CALL( CPXsetterminate, m_env, &AbortOptimization );

	// setup iteration display
	display.addColumn("iter", 0, 6);
	display.addColumn("stage", 1, 6);
	display.addColumn("alpha", 2, 10);
	display.addColumn("origObj", 3, 20);
	display.addColumn("#frac", 6, 10);
	display.addColumn("sumfrac", 7, 10);
	display.addColumn("dist", 10, 10);
	display.addColumn("P", 11, 2);
	display.addColumn("#flips", 12, 8);
	display.addColumn("time", 15, 10);

	// find first fractional solution (or use user supplied one)
	if (xStart.size())
	{
		DOMINIQS_ASSERT( xStart.size() == model.numVars );
		frac_x = xStart;
		primalFeas = pFeas;
	}
	else solveInitialLP();
	gLog().logItem("startNumFrac", solutionNumFractional(integers, frac_x, integralityEps));

	gLog().setConsoleEcho(false);
	// setup for changing objective function
	CPXchgobjsen(m_env, m_lp, CPX_MIN); //< change obj sense: minimize distance
	if (reOptIterLimit >= 0) { CHECKED_CPX_CALL( CPXsetintparam, m_env, CPX_PARAM_ITLIM, reOptIterLimit ); }
	double runningAlpha = alpha;
	if (isNull(objNorm))
	{
		objNorm = 1.0;
		runningAlpha = 0.0;
	}

	int stage = 0;
	// stage 1
	std::vector<double> bestPoint;
	bool found = stage1(runningAlpha, bestPoint);
	stage = 1;

	// stage 2
	if (found) foundIncumbent(frac_x, dotProduct(&frac_x[0], &obj[0], n));
	else found = stage2(runningAlpha, bestPoint);
	stage = 2;
	// stage 3
	if (found) foundIncumbent(frac_x, dotProduct(&frac_x[0], &obj[0], n));
	else if (doStage3)
	{
		found = stage3(bestPoint);
		stage = 3;
	}
	if (found) foundIncumbent(frac_x, dotProduct(&frac_x[0], &obj[0], n));

	// restore original objective function
	std::vector<int> colIndices(n);
	dominiqs::iota(colIndices.begin(), colIndices.end(), 0);
	CHECKED_CPX_CALL( CPXchgobj, m_env, m_lp, obj.size(), &colIndices[0], &obj[0] );
	// restore signal handler
	::signal(SIGINT, previousHandler);
	chrono.stop();

	m_lp = 0;
	m_env = 0;

	gLog().setConsoleEcho(true);
	GlobalAutoSection statS("stats");
	gLog().logItem("found", found);
	gLog().logItem("stage", stage);
	gLog().logItem("totalLpTime", lpChrono.getTotal());
	gLog().logItem("totalRoundingTime", roundChrono.getTotal());
	gLog().logItem("iterations", nitr);
	gLog().logItem("time", chrono.getTotal());
	gLog().logItem("perturbationCnt", pertCnt);
	gLog().logItem("restartCnt", restartCnt);

	this->totalTime = chrono.getTotal();
	this->finalStage = stage;
	return found;
}

// feasiblity pump helpers

void FeasibilityPump::solveInitialLP()
{
	int n = model.numVars;
	// solve initial LP
	CHECKED_CPX_CALL( CPXsetintparam, m_env, CPX_PARAM_SCRIND, CPX_ON );
	if (logLP) { CHECKED_CPX_CALL( CPXwriteprob, m_env, m_lp, format("first.lp").str().c_str(), 0 ); }
	if (optIterLimit >= 0) CHECKED_CPX_CALL( CPXsetintparam, m_env, CPX_PARAM_ITLIM, optIterLimit );
	double timeLeft = std::max(timeLimit - chrono.getElapsed(), 0.0);
	CHECKED_CPX_CALL( CPXsetdblparam, m_env, CPX_PARAM_TILIM, timeLeft );
	switch(firstOptMethod)
	{
		case 'S': CHECKED_CPX_CALL( CPXlpopt, m_env, m_lp ); break;
		case 'P': CHECKED_CPX_CALL( CPXprimopt, m_env, m_lp ); break;
		case 'D': CHECKED_CPX_CALL( CPXdualopt, m_env, m_lp ); break;
		case 'B': CHECKED_CPX_CALL( CPXbaropt, m_env, m_lp ); break;
	}
	MYLOG( logDetails,
		gLog().logItem("cpxIterations", CPXgetitcnt(m_env, m_lp));
		gLog().logItem("cpxMethod", CPXgetmethod(m_env, m_lp));
	);
	gLog().logItem("relaxationTime", chrono.getElapsed());
	if (emitSolvedLP) emitSolvedLP(m_env, m_lp, 0);
	CHECKED_CPX_CALL( CPXgetx, m_env, m_lp, &frac_x[0], 0, n - 1 );
	relaxationValue = dotProduct(&frac_x[0], &obj[0], n);
	gLog().logItem("relaxationObjValue", relaxationValue);
	CHECKED_CPX_CALL( CPXsolninfo, m_env, m_lp, 0, 0, &primalFeas, 0 );
	CHECKED_CPX_CALL( CPXsetintparam, m_env, CPX_PARAM_SCRIND, CPX_OFF );
}

void FeasibilityPump::perturbe(std::vector<double>& x, bool ignoreGeneralIntegers)
{
	MYLOG( logPerturbation, gLog().startSection("perturbe"); );
	pertCnt++;
	// order
	std::multimap< double, int, std::greater<double> > toOrder;
	double sigma;
	if (ignoreGeneralIntegers)
	{
		for (int j: binaries)
		{
			if (perturbeFromTransformerOutput) sigma = fabs(x[j] - frac_x[j]);
			else sigma = fabs(floor(frac_x[j] + 0.5) - frac_x[j]);
			if (sigma > sigmaThr) toOrder.insert(std::pair<double, int>(sigma, j));
		}
	}
	else
	{
		for (int j: integers)
		{
			if (perturbeFromTransformerOutput) sigma = fabs(x[j] - frac_x[j]);
			else sigma = fabs(floor(frac_x[j] + 0.5) - frac_x[j]);
			if (sigma > sigmaThr) toOrder.insert(std::pair<double, int>(sigma, j));
		}
	}
	if (toOrder.empty()) return;

	// flip
	int nflips = avgFlips * (rnd.getFloat() + 0.5);
	int flipsDone = 0;
	std::multimap<double, int>::const_iterator itr = toOrder.begin();
	std::multimap<double, int>::const_iterator end = toOrder.end();
	while ((itr != end) && (flipsDone < nflips))
	{
		int toFlip = itr->second;
		if (equal(x[toFlip], model.xLb[toFlip], integralityEps)) { x[toFlip] += 1.0; ++flipsDone; }
		else if (equal(x[toFlip], model.xUb[toFlip], integralityEps)) { x[toFlip] -= 1.0; ++flipsDone; }
		else
		{
			// this can happen only with general integer variables
			if (lessThan(x[toFlip], frac_x[toFlip], integralityEps)) { x[toFlip] += 1.0; ++flipsDone; }
			if (greaterThan(x[toFlip], frac_x[toFlip], integralityEps)) { x[toFlip] -= 1.0; ++flipsDone; }
		}
		++itr;
	}
	DOMINIQS_ASSERT( flipsDone );
	display.set("P", IT_DISPLAY_SIMPLE_FMT(char, '*'));
	display.set("#flips", IT_DISPLAY_SIMPLE_FMT(int, flipsDone));
	MYLOG( logPerturbation,
		gLog().logItem("maxFlips", nflips);
		gLog().logItem("flipsDone", flipsDone);
		gLog().endSection();
	);
}

void FeasibilityPump::restart(std::vector<double>& x, bool ignoreGeneralIntegers)
{
	MYLOG( logPerturbation, gLog().startSection("restart"); );
	restartCnt++;
	// get previous solution
	DOMINIQS_ASSERT( lastIntegerX.size() );
	const std::vector<double>& previousSol = lastIntegerX.begin()->second;
	// perturbe
	double sigma;
	double r;
	// perturbe binaries
	int changed = 0;
	unsigned int size = binaries.size();
	for (unsigned int i = 0; i < size; i++)
	{
		int j = binaries[i];
		r = rnd.getFloat() - 0.47;
		if ( r > 0 && equal(x[j], previousSol[j], integralityEps) )
		{
			if (perturbeFromTransformerOutput) sigma = fabs(x[j] - frac_x[j]);
			else sigma = fabs(floor(frac_x[j] + 0.5) - frac_x[j]);
			if ( (sigma + r) > 0.5 )
			{
				x[j] = isNull(x[j], integralityEps) ? 1.0 : 0.0;
				++changed;
			}
		}
	}

	if (!ignoreGeneralIntegers && gintegers.size())
	{
		if (lastRestart < nitr)
		{
			while (lastRestart++ < nitr) flipsInRestart *= GEOM_FACTOR; // geometric decrease
		}
		flipsInRestart = std::min(flipsInRestart + 2 * avgFlips + 1, maxFlipsInRestart); // linear increase
		DOMINIQS_ASSERT( flipsInRestart );
		// perturbe general integers
		double newValue;
		for (int i = 0; i < flipsInRestart; i++)
		{
			int randIdx = int(rnd.getFloat() * (gintegers.size() - 1));
			DOMINIQS_ASSERT( randIdx >= 0 && randIdx < (int)gintegers.size() );
			int j = gintegers[randIdx];
			double lb = model.xLb[i];
			double ub = model.xUb[i];
			r = rnd.getFloat();
			if( (ub - lb) < BIGBIGM ) newValue = floor(lb + (1 + ub - lb) * r);
			else if( (x[j] - lb) < BIGM ) newValue = lb + (2 * BIGM - 1) * r;
			else if( (ub - x[j]) < BIGM ) newValue = ub - (2 * BIGM - 1) * r;
			else newValue = x[j] + (2 * BIGM - 1) * r - BIGM;
			newValue = std::max(std::min(newValue, ub), lb);
			if( different(newValue, x[j], integralityEps) )
			{
				x[j] = newValue;
				++changed;
			}
		}
		DOMINIQS_ASSERT( changed );
	}
	MYLOG( logPerturbation,
		gLog().logItem("flipsInRestart", flipsInRestart);
		gLog().logItem("flipsDone", changed);
		gLog().endSection();
	);
}

bool FeasibilityPump::stage1(double& runningAlpha, std::vector<double>& bestPoint)
{
	// setup
	GlobalAutoSection autoS("stage1");

	// setup data structures for changing the objective function
	int n = CPXgetnumcols(m_env, m_lp);
	std::vector<double> distObj(n, 0);
	std::vector<int> colIndices(n);
	dominiqs::iota(colIndices.begin(), colIndices.end(), 0);

	// first rounding
	roundChrono.start();
	frac2int->apply(frac_x, integer_x);
	roundChrono.stop();

	bestPoint = integer_x;
	lastIntegerX.push_front(AlphaVector(runningAlpha, integer_x));
	double bestDist = CPX_INFBOUND;
	bool foundBinary = (primalFeas && isSolutionInteger(binaries, frac_x, integralityEps));
	if (foundBinary)
	{
		bestDist = 0.0;
		bestPoint = frac_x;
	}

	int missedDecrease = 0;
	int stage = 1;

	// stage 1 loop
	while (!AbortOptimization
		&& (!foundBinary)
		&& (chrono.getElapsed() < timeLimit)
		&& (restartCnt < maxRestarts)
		&& (missedDecrease < s1MaxMissedDecrease)
		&& (nitr < s1IterLimit))
	{
		nitr++;
		display.resetIteration();
		if (display.needHeader(nitr)) display.printHeader(std::cout);
		MYLOG( logIteration, gLog().startSection("iteration", "number", nitr); );

		// int -> frac
		lpChrono.start();
		runningAlpha *= alphaFactor;
		MYLOG( logDetails, gLog().logItem("alpha", runningAlpha); );
		fill(distObj.begin(), distObj.end(), 0.0);
		for (int j: binaries) distObj[j] = (isNull(integer_x[j], integralityEps) ? (1.0 - runningAlpha) : (runningAlpha - 1.0));
		accumulate(&distObj[0], &obj[0], n, (runningAlpha * sqrt(binaries.size()) / objNorm));
		CHECKED_CPX_CALL( CPXchgobj, m_env, m_lp, distObj.size(), &colIndices[0], &distObj[0] );

		// solve LP
		if (logLP) { CHECKED_CPX_CALL( CPXwriteprob, m_env, m_lp, (format("distLp_%1%.lp") % nitr).str().c_str(), 0 ); }
		double timeLeft = std::max(timeLimit - chrono.getElapsed(), 0.0);
		CHECKED_CPX_CALL( CPXsetdblparam, m_env, CPX_PARAM_TILIM, timeLeft );
		switch(reOptMethod)
		{
			case 'S': CHECKED_CPX_CALL( CPXlpopt, m_env, m_lp ); break;
			case 'P': CHECKED_CPX_CALL( CPXprimopt, m_env, m_lp ); break;
			case 'D': CHECKED_CPX_CALL( CPXdualopt, m_env, m_lp ); break;
			case 'B': CHECKED_CPX_CALL( CPXbaropt, m_env, m_lp ); break;
		}
		MYLOG( logDetails,
			gLog().logItem("cpxIterations", CPXgetitcnt(m_env, m_lp));
			gLog().logItem("cpxMethod", CPXgetmethod(m_env, m_lp));
		);
		MYLOG( logTime, gLog().logItem("lpTime", lpChrono.getPartial()); );
		if (emitSolvedLP) emitSolvedLP(m_env, m_lp, stage);

		lpChrono.stop();
		// get solution and some statistics
		CHECKED_CPX_CALL( CPXgetx, m_env, m_lp, &frac_x[0], 0, n - 1 );
		CHECKED_CPX_CALL( CPXsolninfo, m_env, m_lp, 0, 0, &primalFeas, 0 );
		double origObj = dotProduct(&obj[0], &frac_x[0], n);
		double dist = solutionsDistance(binaries, frac_x, integer_x);
		int numFrac = solutionNumFractional(binaries, frac_x, integralityEps);
		double frac = solutionFractionality(binaries, frac_x);
		MYLOG( logIteration, gLog().logItem("distance", dist); );
		MYLOG( logIteration, gLog().logItem("obj", origObj); );
		MYLOG( logIteration, gLog().logItem("binaryFractionality", frac); );
		MYLOG( logIteration, gLog().logItem("numFrac", numFrac); );

		// frac -> int
		roundChrono.start();
		frac2int->apply(frac_x, integer_x);
		roundChrono.stop();
		MYLOG( logTime, gLog().logItem("roundingTime", roundChrono.getPartial()); );

		// save as best point if sufficient distance decrease
		if (dist < bestDist)
		{
			if (dist / bestDist < 0.9) missedDecrease = 0;
			bestDist = dist;
			bestPoint = integer_x;
		}
		else missedDecrease++;

		// check if solution is feasible w.r.t integrality constraint on binaries only
		if (primalFeas && isSolutionInteger(binaries, frac_x, integralityEps))
		{
			foundBinary = true;
			bestDist = 0.0;
			bestPoint = integer_x;
		}

		if (!foundBinary)
		{
			// cycle detection and antistalling actions
			// is it the same of the last one? If yes perturbe
			if (lastIntegerX.size()
				&& areSolutionsEqual(integers, integer_x, (*(lastIntegerX.begin())).second, integralityEps)
				&& equal(runningAlpha, (*(lastIntegerX.begin())).first, alphaDist))
			{
				MYLOG( logDetails, gLog().logMsg("sameAsLast"); );
				perturbe(integer_x, true);
			}
			// do a restart until we are able to insert it in the cache
			while (true)
			{
				if (isInCache(runningAlpha, integer_x, true)) restart(integer_x, true);
				else break;
			}
		}
		lastIntegerX.push_front(AlphaVector(runningAlpha, integer_x));

		if (display.needPrint(nitr))
		{
			display.set("stage", IT_DISPLAY_SIMPLE_FMT(int, stage));
			display.set("iter", IT_DISPLAY_SIMPLE_FMT(int, nitr));
			display.set("alpha", IT_DISPLAY_SIMPLE_FLOAT_FMT(double, runningAlpha, EXTENDED_PRECISION));
			display.set("time", IT_DISPLAY_SIMPLE_FMT(double, chrono.getElapsed()));
			display.set("origObj", IT_DISPLAY_SIMPLE_FMT(double, origObj));
			display.set("dist", IT_DISPLAY_SIMPLE_FLOAT_FMT(double, dist, EXTENDED_PRECISION));
			display.set("#frac", IT_DISPLAY_SIMPLE_FMT(int, numFrac));
			display.set("sumfrac", IT_DISPLAY_SIMPLE_FLOAT_FMT(double, frac, EXTENDED_PRECISION));
			display.printIteration(std::cout);
		}

		MYLOG( logIteration, gLog().endSection(); );
	}
	gLog().logItem("s1Iterations", nitr);
	return (primalFeas && isSolutionInteger(integers, frac_x, integralityEps));
}

bool FeasibilityPump::stage2(double& runningAlpha, std::vector<double>& bestPoint)
{
	// setup
	GlobalAutoSection autoS("stage2");
	int stage = 2;
	lastIntegerX.clear();
	frac2int->ignoreGeneralIntegers(false);
	int n = CPXgetnumcols(m_env, m_lp);
	std::vector<double> distObj(n, 0);
	std::vector<int> colIndices(n);
	dominiqs::iota(colIndices.begin(), colIndices.end(), 0);
	double bestDist = CPX_INFBOUND;
	integer_x = bestPoint;
	int missedDecrease = 0;
	int s1restarts = restartCnt;
	maxFlipsInRestart = std::max(int(gintegers.size() / 10.0), 10);
	gLog().logItem("maxFlipsInRestart", maxFlipsInRestart);
	bool found = false;

	// stage 2 loop
	while (!AbortOptimization
		&& (!found)
		&& (chrono.getElapsed() < timeLimit)
		&& (missedDecrease < maxMissedDecrease)
		&& ((restartCnt - s1restarts) < maxRestarts)
		&& (nitr < iterLimit))
	{
		nitr++;
		display.resetIteration();
		if (display.needHeader(nitr)) display.printHeader(std::cout);
		MYLOG( logIteration, gLog().startSection("iteration", "number", nitr); );

		// int -> frac
		lpChrono.start();
		runningAlpha *= alphaFactor;

		// setup distance objective
		int addedVars = 0;
		int addedConstrs = 0;
		fill(distObj.begin(), distObj.end(), 0.0);
		for (int j: binaries) distObj[j] = (isNull(integer_x[j], integralityEps) ? (1.0 - runningAlpha) : (runningAlpha - 1.0));
		for (int j: gintegers)
		{
			if (equal(integer_x[j], model.xLb[j], integralityEps)) distObj[j] = (1.0 - runningAlpha);
			else if (equal(integer_x[j], model.xUb[j], integralityEps)) distObj[j] = (runningAlpha - 1.0);
			else
			{
				// add auxiliary variable
				std::string deltaName = model.xNames[j] + "_delta";
				char* cname = (char*)(deltaName.c_str());
				CHECKED_CPX_CALL( CPXnewcols, m_env, m_lp, 1, 0, 0, 0, 0, &cname );
				int auxIdx = CPXgetnumcols(m_env, m_lp) - 1;
				colIndices.push_back(auxIdx);
				distObj.push_back(1.0 - runningAlpha);
				addedVars++;
				// add constraints
				SparseVector vec;
				vec.push(j, 1.0);
				vec.push(auxIdx, -1.0);
				addCut(m_env, m_lp, model.xNames[j] + "_d1", vec, 'L', integer_x[j]);
				vec.coef()[1] = 1.0;
				addCut(m_env, m_lp, model.xNames[j] + "_d2", vec, 'G', integer_x[j]);
				addedConstrs += 2;
			}
		}
		MYLOG( logDetails, gLog().logItem("addedVars", addedVars); );
		MYLOG( logDetails, gLog().logItem("addedConstrs", addedConstrs); );
		DOMINIQS_ASSERT( distObj.size() == (unsigned int)(n + addedVars) );
		DOMINIQS_ASSERT( distObj.size() == colIndices.size() );
		accumulate(&distObj[0], &obj[0], n, (runningAlpha * sqrt(integers.size()) / objNorm));
		CHECKED_CPX_CALL( CPXchgobj, m_env, m_lp, distObj.size(), &colIndices[0], &distObj[0] );
		if (logLP) { CHECKED_CPX_CALL( CPXwriteprob, m_env, m_lp, (format("distLp_%1%.lp") % nitr).str().c_str(), 0 ); }

		double timeLeft = std::max(timeLimit - chrono.getElapsed(), 0.0);
		CHECKED_CPX_CALL( CPXsetdblparam, m_env, CPX_PARAM_TILIM, timeLeft );
		// solve LP
		switch(reOptMethod)
		{
			case 'S': CHECKED_CPX_CALL( CPXlpopt, m_env, m_lp ); break;
			case 'P': CHECKED_CPX_CALL( CPXprimopt, m_env, m_lp ); break;
			case 'D': CHECKED_CPX_CALL( CPXdualopt, m_env, m_lp ); break;
			case 'B': CHECKED_CPX_CALL( CPXbaropt, m_env, m_lp ); break;
		}
		MYLOG( logDetails,
			gLog().logItem("cpxIterations", CPXgetitcnt(m_env, m_lp));
			gLog().logItem("cpxMethod", CPXgetmethod(m_env, m_lp));
		);
		lpChrono.stop();
		if (emitSolvedLP) emitSolvedLP(m_env, m_lp, stage);
		CHECKED_CPX_CALL( CPXgetx, m_env, m_lp, &frac_x[0], 0, n - 1 );
		CHECKED_CPX_CALL( CPXsolninfo, m_env, m_lp, 0, 0, &primalFeas, 0 );
		double origObj = dotProduct(&obj[0], &frac_x[0], n);

		// cleanup added vars and constraints
		if (addedConstrs)
		{
			int begin = CPXgetnumrows(m_env, m_lp) - addedConstrs;
			CHECKED_CPX_CALL( CPXdelrows, m_env, m_lp, begin, begin + addedConstrs - 1);
		}
		if (addedVars)
		{
			int begin = CPXgetnumcols(m_env, m_lp) - addedVars;
			CHECKED_CPX_CALL( CPXdelcols, m_env, m_lp, begin, begin + addedVars - 1);
		}
		colIndices.resize(n);
		distObj.resize(n);
		DOMINIQS_ASSERT( CPXgetnumcols(m_env, m_lp) == n );

		// get some statistics
		double dist = solutionsDistance(integers, frac_x, integer_x);
		double frac = solutionFractionality(integers, frac_x);
		MYLOG( logIteration, gLog().logItem("obj", origObj); );
		MYLOG( logIteration, gLog().logItem("distance", dist); );
		MYLOG( logIteration, gLog().logItem("fractionality", frac); );
		int numFrac = solutionNumFractional(integers, frac_x, integralityEps);

		MYLOG( logTime, gLog().logItem("lpTime", lpChrono.getPartial()); );

		// frac -> int
		roundChrono.start();
		frac2int->apply(frac_x, integer_x);
		roundChrono.stop();
		MYLOG( logTime, gLog().logItem("roundingTime", roundChrono.getPartial()); );

		// save as best point if sufficient distance decrease
		if (dist < bestDist)
		{
			if (dist / bestDist < 0.9) missedDecrease = 0;
			bestDist = dist;
			bestPoint = integer_x;
		}
		else missedDecrease++;

		// check if feasible
		found = (primalFeas && isSolutionInteger(integers, frac_x, integralityEps));

		if (!found)
		{
			// cycle detection and antistalling actions
			// is it the same of the last one? If yes perturbe
			if (lastIntegerX.size()
				&& areSolutionsEqual(integers, integer_x, (*(lastIntegerX.begin())).second, integralityEps)
				&& equal(runningAlpha, (*(lastIntegerX.begin())).first, alphaDist))
			{
				MYLOG( logDetails, gLog().logMsg("sameAsLast"); );
				perturbe(integer_x, false);
			}
			// do a restart until we are able to insert it in the cache
			while (true)
			{
				if (isInCache(runningAlpha, integer_x, false)) restart(integer_x, false);
				else break;
			}
		}
		lastIntegerX.push_front(AlphaVector(runningAlpha, integer_x));

		if (display.needPrint(nitr))
		{
			display.set("stage", IT_DISPLAY_SIMPLE_FMT(int, stage));
			display.set("iter", IT_DISPLAY_SIMPLE_FMT(int, nitr));
			display.set("alpha", IT_DISPLAY_SIMPLE_FLOAT_FMT(double, runningAlpha, EXTENDED_PRECISION));
			display.set("origObj", IT_DISPLAY_SIMPLE_FMT(double, origObj));
			display.set("time", IT_DISPLAY_SIMPLE_FMT(double, chrono.getElapsed()));
			display.set("dist", IT_DISPLAY_SIMPLE_FLOAT_FMT(double, dist, EXTENDED_PRECISION));
			display.set("#frac", IT_DISPLAY_SIMPLE_FMT(int, numFrac));
			display.set("sumfrac", IT_DISPLAY_SIMPLE_FLOAT_FMT(double, frac, EXTENDED_PRECISION));
			display.printIteration(std::cout);
		}

		MYLOG( logIteration, gLog().endSection(); );
	}
	return found;
}

bool FeasibilityPump::stage3(const std::vector<double>& bestPoint)
{
	if (AbortOptimization) return false;
	GlobalAutoSection autoS("stage3");
	double elapsedTime = chrono.getElapsed();
	double remainingTime = timeLimit - elapsedTime;
	double s3TimeLimit = std::max(std::min(remainingTime, elapsedTime), 1.0);
	MYLOG( logDetails, gLog().logItem("s3TimeLimit", s3TimeLimit); );
	if (lessThan(remainingTime, 0.1)) return false;

	int n = CPXgetnumcols(m_env, m_lp);
	bool found = false;
	// restore type information
	std::vector<char> ctype(n, 'C');
	for (int j: binaries) ctype[j] = 'B';
	for (int j: gintegers) ctype[j] = 'I';
	CHECKED_CPX_CALL( CPXcopyctype, m_env, m_lp, &ctype[0] );
	DOMINIQS_ASSERT( CPXgetprobtype(m_env, m_lp) == CPXPROB_MILP );
	// load best point and generate objective
	integer_x = bestPoint;
	int addedVars = 0;
	int addedConstrs = 0;
	std::vector<double> distObj(n, 0.0);
	std::vector<int> colIndices(n);
	dominiqs::iota(colIndices.begin(), colIndices.end(), 0);
	for (int j: binaries) distObj[j] = (isNull(integer_x[j], integralityEps) ? 1.0 : 1.0);
	for (int j: gintegers)
	{
		if (equal(integer_x[j], model.xLb[j], integralityEps)) distObj[j] = 1.0;
		else if (equal(integer_x[j], model.xUb[j], integralityEps)) distObj[j] = -1.0;
		else
		{
			// add auxiliary variable
			std::string deltaName = model.xNames[j] + "_delta";
			char* cname = (char*)(deltaName.c_str());
			CHECKED_CPX_CALL( CPXnewcols, m_env, m_lp, 1, 0, 0, 0, 0, &cname );
			int auxIdx = CPXgetnumcols(m_env, m_lp) - 1;
			colIndices.push_back(auxIdx);
			distObj.push_back(1.0);
			addedVars++;
			// add constraints
			SparseVector vec;
			vec.push(j, 1.0);
			vec.push(auxIdx, -1.0);
			addCut(m_env, m_lp, model.xNames[j] + "_d1", vec, 'L', integer_x[j]);
			vec.coef()[1] = 1.0;
			addCut(m_env, m_lp, model.xNames[j] + "_d2", vec, 'G', integer_x[j]);
			addedConstrs += 2;
		}
	}
	MYLOG( logDetails, gLog().logItem("addedVars", addedVars); );
	MYLOG( logDetails, gLog().logItem("addedConstrs", addedConstrs); );
	DOMINIQS_ASSERT( distObj.size() == (unsigned int)(n + addedVars) );
	DOMINIQS_ASSERT( distObj.size() == colIndices.size() );
	CHECKED_CPX_CALL( CPXchgobj, m_env, m_lp, distObj.size(), &colIndices[0], &distObj[0] );
	// CHECKED_CPX_CALL( CPXwriteprob, m_env, m_lp, "stage3Lp.lp", 0 );
	CHECKED_CPX_CALL( CPXsetintparam, m_env, CPX_PARAM_SCRIND, CPX_ON );
	CHECKED_CPX_CALL( CPXsetintparam, m_env, CPX_PARAM_INTSOLLIM, 1 );
	CHECKED_CPX_CALL( CPXsetdblparam, m_env, CPX_PARAM_WORKMEM, 4000.0 ); //< 4GB
	CHECKED_CPX_CALL( CPXsetintparam, m_env, CPX_PARAM_NODEFILEIND, 3 );
	CHECKED_CPX_CALL( CPXsetdblparam, m_env, CPX_PARAM_TILIM, timeLimit );
	CPXmipopt(m_env, m_lp);
	CHECKED_CPX_CALL( CPXsolninfo, m_env, m_lp, 0, 0, &primalFeas, 0 );
	if (primalFeas)
	{
		CHECKED_CPX_CALL( CPXgetx, m_env, m_lp, &frac_x[0], 0, n - 1 );
		DOMINIQS_ASSERT( isSolutionInteger(integers, frac_x, integralityEps) );
	}
	// cleanup added vars and constraints
	if (addedConstrs)
	{
		int begin = CPXgetnumrows(m_env, m_lp) - addedConstrs;
		CHECKED_CPX_CALL( CPXdelrows, m_env, m_lp, begin, begin + addedConstrs - 1);
	}
	if (addedVars)
	{
		int begin = CPXgetnumcols(m_env, m_lp) - addedVars;
		CHECKED_CPX_CALL( CPXdelcols, m_env, m_lp, begin, begin + addedVars - 1);
	}
	colIndices.resize(n);
	distObj.resize(n);
	// improvement phase
	if (primalFeas)
	{
		// fix integer variables
		std::vector<char> lu(integers.size(), 'B');
		std::vector<double> val(integers.size());
		for (unsigned int i = 0; i < integers.size(); i++) val[i] = frac_x[integers[i]];
		CHECKED_CPX_CALL( CPXchgbds, m_env, m_lp, integers.size(), &integers[0], &lu[0], &val[0] );
		// restore original obj
		CHECKED_CPX_CALL( CPXchgobj, m_env, m_lp, obj.size(), &colIndices[0], &obj[0] );
		// solve
		CHECKED_CPX_CALL( CPXmipopt, m_env, m_lp );
		CHECKED_CPX_CALL( CPXsolninfo, m_env, m_lp, 0, 0, &primalFeas, 0 );
		if (primalFeas) { CHECKED_CPX_CALL( CPXgetx, m_env, m_lp, &frac_x[0], 0, n - 1 ); }
		else
		{
			// this should happen only for numerical problems!
			gLog().logMsg("No solution after improvement phase: numerical problems!!!");
		}
		found = true;
	}
	return found;
}

void FeasibilityPump::foundIncumbent(const std::vector<double>& x, double objval)
{
	incumbent = x;
	incumbentValue = objval;
	if (emitNewIncumbent) emitNewIncumbent(incumbentValue, chrono.getElapsed(), nitr);
	MYLOG( logDetails,
	        gLog().startSection("newIncumbent");
	        gLog().logItem("value", incumbentValue);
	        gLog().endSection();
        );
	frac2int->newIncumbent(incumbent, incumbentValue);
	lastIntegerX.clear();
}

bool FeasibilityPump::isInCache(double a, const std::vector<double>& x, bool ignoreGeneralIntegers)
{
	bool found = false;
	std::list< AlphaVector >::const_iterator itr = lastIntegerX.begin();
	std::list< AlphaVector >::const_iterator end = lastIntegerX.end();
	if (ignoreGeneralIntegers)
	{
		while ((itr != end) && !found)
		{
			if ((fabs(a - itr->first) < alphaDist) && areSolutionsEqual(binaries, itr->second, integer_x, integralityEps)) found = true;
			++itr;
		}
	}
	else
	{
		while ((itr != end) && !found)
		{
			if ((fabs(a - itr->first) < alphaDist) && areSolutionsEqual(integers, itr->second, integer_x, integralityEps)) found = true;
			++itr;
		}
	}
	return found;
}

} // namespace dominiqs
