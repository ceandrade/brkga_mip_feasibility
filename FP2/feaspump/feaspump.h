/**
 * @file feaspump.h
 * @brief Feasibility Pump algorithm Header
 *
 * @author Domenico Salvagnin <dominiqs at gmail dot com>
 * 2008
 */

#ifndef FEASPUMP_H
#define FEASPUMP_H

#include <list>
#include <boost/function.hpp>

#include <utils/randgen.h>
#include <utils/it_display.h>

#include "fp_interface.h"

namespace dominiqs {

/**
 * @brief Basic Feasibility Pump Scheme
 * Accepts custom rounders
 */

class FeasibilityPump
{
public:
	FeasibilityPump();
	// signals
	boost::function<void (double, double, int)> emitNewIncumbent; // parameters: value, time, iteration
	boost::function<void (Env env, Prob lp, int stage)> emitSolvedLP; // emitted whenever an LP is solved
	// config
	void readConfig();
	/** init algorithm
	 * @param env: cplex environment
	 * @param lp: problem object (this is modified by the algorithm: you may want to pass a copy!)
	 * @param ctype: if the problem object is an LP, you can provide variable type info with this vector
	 */
	void init(Env env, Prob lp, const std::vector<char>& ctype = std::vector<char>());
	/** pump
	 * @param xStart: starting fractional solution (will solve LP if empty)
	 * @param pFeas: primal feasiblity status of supplied vector
	 */
	bool pump(const std::vector<double>& xStart = std::vector<double>(), bool pFeas = false);
	// get solution info
	void getSolution(std::vector<double>& x) const;
	double getSolutionValue(const std::vector<double>& x) const;
	int getIterations() const;
	// reset
	void reset();
private:
	// FP options
	double timeLimit;
	int s1IterLimit;
	int iterLimit;
	int s1MaxMissedDecrease;
	int maxMissedDecrease;
	int maxRestarts;
	int avgFlips;
	double integralityEps;
	uint64_t seed;
	double sigmaThr;
	bool perturbeFromTransformerOutput;
	double alpha;
	double alphaFactor;
	double alphaDist;
	bool doStage3;
	// LP options
	char firstOptMethod;
	int optIterLimit;
	char reOptMethod;
	int reOptIterLimit;
	// Log options
	bool logPerturbation;
	bool logDetails;
	bool logIteration;
	bool logTime;
	bool logLP;
	// FP data
	Env m_env;
	Prob m_lp;
	SolutionTransformerPtr frac2int; /**< rounder */
	std::vector<double> frac_x; /**< fractional x^* */
	int primalFeas; /**< is current fractional x^* primal feasible? */
	std::vector<double> integer_x; /**< integer x^~ */
	typedef std::pair< double, std::vector<double> > AlphaVector;
	std::list< AlphaVector > lastIntegerX; /**< integer x cache */
	double relaxationValue;
	std::vector<double> incumbent; /**< current incumbent */
	double incumbentValue;
	UnitRandGen rnd;
	// problem data
	Model model;
	bool isPureInteger; /**< is it a mixed linear program? */
	bool isBinary; /**< are there general integers? */
	std::vector<double> obj; /**< original objective function */
	double objNorm;
	int objsen;
	std::vector<bool> fixed; /**< fixed status in the original formulation (usually due to presolve) */
	std::vector<int> binaries; /**< list of binary vars indexes */
	std::vector<int> gintegers; /**< list of general integer vars indexes */
	std::vector<int> integers; /**< list of non continuous vars indexes (binaries + gintegers) */
	// stats
	IterationDisplay display;
	int pertCnt;
	int restartCnt;
	int nitr; /**< pumping iterations */
	int lastRestart;
	int flipsInRestart;
	int maxFlipsInRestart;
	Chrono chrono;
	Chrono lpChrono;
	Chrono roundChrono;
	// helpers
	void solveInitialLP();
	void perturbe(std::vector<double>& x, bool ignoreGeneralIntegers);
	void restart(std::vector<double>& x, bool ignoreGeneralIntegers);
	bool stage1(double& runningAlpha, std::vector<double>& bestPoint);
	bool stage2(double& runningAlpha, std::vector<double>& bestPoint);
	bool stage3(const std::vector<double>& bestPoint);
	void foundIncumbent(const std::vector<double>& x, double objval);
	bool isInCache(double a, const std::vector<double>& x, bool ignoreGeneralIntegers);

// Carlos
public:
	double totalTime;
	int finalStage;
};

} // namespace dominiqs

#endif /* FEASPUMP_H */
