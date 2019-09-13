/**
 * @file gomory.cpp
 * @brief Gomory cuts separators
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * Copyright 2009 Domenico Salvagnin
 */

//#define DOMINIQS_DEBUG

#include "gomory.h"

#include <algorithm>
#include <limits>
#include <iomanip>

#include <boost/format.hpp>

#include <utils/maths.h>
#include <utils/floats.h>
#include <utils/logger.h>
#include <utils/sorting.h>
#include <utils/chrono.h>
#include <utils/xmlconfig.h>

namespace dominiqs {

static const double bigRange = 1e4;

static inline double GMIcoef(double alpha, char type, double f0, double ratio, double eps) /* ratio = f0 / (1 - f0) */
{
	if (type != 'C')
	{
		// integral
		double fj = fractionalPart(alpha, eps);
		return (fj > f0) ? (1.0 - fj) * ratio : fj;
	}
	else
	{
		// continuous
		return isNegative(alpha, eps) ? (-alpha * ratio) : alpha;
	}
	return 0.0; // never goes here!
}

void printCut(std::ostream& out, const Model& m, const Cut& c)
{
	out << c.name << ": ";
	const int* idx = c.row.idx();
	const double* coef = c.row.coef();
	for (unsigned int i = 0; i < c.row.size(); i++)
	{
		out << coef[i] << " " << m.xNames[idx[i]] << " ";
	}
	if (c.sense == 'L') out << "<= ";
	else if (c.sense == 'G') out << ">= ";
	else out << "== ";
	out << c.rhs << std::endl;
}

static const double MIN_VIOLATION_DEF = 0.001;
static const double MAX_SUPPORT_DEF = 1.0;
static const double MAX_DYNAMISM_DEF = 1E+10;
static const int MAX_RANK_DEF = 1000;
static const bool FORCE_REFACTOR_DEF = true;
static const int MAX_GMI_DEF = 200;
static const bool ONLY_VIOLATED_DEF = false;
static const double ONLY_VIOL_BAD_DEF = 0.20;

GomorySeparator::GomorySeparator()
		: name("GomorySeparator"), minViolation(MIN_VIOLATION_DEF), maxSupport(MAX_SUPPORT_DEF),
		maxDynamism(MAX_DYNAMISM_DEF), maxRank(MAX_RANK_DEF), debugOn(false),
		forceRefactor(FORCE_REFACTOR_DEF), maxGMI(MAX_GMI_DEF),
		onlyViolated(ONLY_VIOLATED_DEF), onlyViolBadThr(ONLY_VIOL_BAD_DEF),
	numImprKCuts(0), numBadDynamism(0), numTried(0), cutCounter(0), numBadX(0), numBad(0), badRatio(0.0)
{
	epsSmallVars = 1e-9;
	epsBigVars = epsSmallVars / bigRange;
}

GomorySeparator::~GomorySeparator()
{
	DOMINIQS_DEBUG_LOG( logStats() );
}

void GomorySeparator::readConfig(dominiqs::XMLConfig& config, const std::string& root)
{
	minViolation = config.get(root, "minViolation", MIN_VIOLATION_DEF);
	maxSupport = config.get(root, "maxSupport", MAX_SUPPORT_DEF);
	maxDynamism = config.get(root, "maxDynamism", MAX_DYNAMISM_DEF);
	maxRank = config.get(root, "maxRank", MAX_RANK_DEF);
	forceRefactor = config.get(root, "forceRefactor", FORCE_REFACTOR_DEF);
	maxGMI = config.get(root, "maxGMI", MAX_GMI_DEF);
	onlyViolated = config.get(root, "onlyViolated", ONLY_VIOLATED_DEF);
	onlyViolBadThr = config.get(root, "onlyViolBadThr", ONLY_VIOL_BAD_DEF);
	GlobalAutoSection configS("config", "name", name);
	gLog().logItem(root + ".maxRank", maxRank);
	gLog().logItem(root + ".maxSupport", maxSupport);
	gLog().logItem(root + ".maxDynamism", maxDynamism);
	gLog().logItem(root + ".minViolation", minViolation);
	gLog().logItem(root + ".forceRefactor", forceRefactor);
	gLog().logItem(root + ".maxGMI", maxGMI);
	gLog().logItem(root + ".onlyViolated", onlyViolated);
	gLog().logItem(root + ".onlyViolBadThr", onlyViolBadThr);
}

/**
 * Calculate a i-th tableau row, combining the rows of the original matrix
 * using the i-th row of B^-1 as multipliers.
 * Very small multipliers are cleaned to zero.
 */

bool GomorySeparator::calculateTableauRow(Model& m, const dominiqs::numarray<double>& multipliers)
{
	rank = 0;
	// handle rhs
	beta = dotProduct(&multipliers[0], &shiftedRhs[0], m.numRows);
	// check fractionality of rhs
	double f = integralityViolation(beta);
	if (lessThan(f, minViolation)) return false;
	if (onlyViolated && (badRatio <= onlyViolBadThr))
	{
		double f0  = fractionalPart(beta);
		double ratio = f0 / (1.0 - f0);
		double violation = f0;
		double alpha;
		for (int k = 0; k < numBadX; k++)
		{
			alpha = dotProduct(*(m.cols[badIdx[k]]), &multipliers[0]);
			if (xStatus[badIdx[k]] == 'U') alpha = -alpha;
			alpha = GMIcoef(alpha, badType[k], f0, ratio, epsBigVars);
			// if (xStatus[badIdx[k]] == 'U') alpha = -alpha;
			violation -= alpha * badVal[k];
			if (violation < minViolation) return false;
		}
		for (int k = numBadX; k < numBad; k++)
		{
			alpha = multipliers[badIdx[k]];
			if (m.sense[badIdx[k]] == 'G') alpha = -alpha;
			alpha = GMIcoef(alpha, badType[k], f0, ratio, epsBigVars);
			violation -= alpha * badVal[k];
			if (violation < minViolation) return false;
		}
	}
	// handle structural and artificial variables
	DOMINIQS_DEBUG_LOG( tabRowChrono.start() );
	xCoef.resize(m.numVars);
	xUsed.resize(m.numVars);
	xUsed.zero();
	xSupport.reserve(m.numVars);
	xSupport.clear();
	sCoef.resize(m.numRows);
	sSupport.reserve(m.numRows);
	sSupport.clear();
	for (unsigned int i = 0; i < m.numRows; i++)
	{
		if (isNotNull(multipliers[i], epsBigVars))
		{
			rank = std::max(rank, m.rows[i]->rank);
			const SparseVector& row = m.rows[i]->row;
			const int* idx = row.idx();
			const double* coef = row.coef();
			int kth;
			for (unsigned int k = 0; k < row.size(); k++)
			{
				kth = idx[k];
				if (!xUsed[kth])
				{
					xUsed[kth] = 1;
					xSupport.push_back_unsafe(kth);
					xCoef[kth] = 0.0;
				}
				xCoef[kth] += (multipliers[i] * coef[k]);
			}
			if (m.sense[i] == 'E') continue;
			if (m.sense[i] == 'G') sCoef[i] = -multipliers[i];
			else sCoef[i] = multipliers[i];
			sSupport.push_back_unsafe(i);
		}
	}
	rank += 1;
	DOMINIQS_DEBUG_LOG( tabRowChrono.stop() );

	if (rank > maxRank) return false;
	int maxSize = std::max(int(maxSupport * m.numVars), 100);
	if (xSupport.size() > (unsigned int)maxSize) return false;
	return true;
}

/**
 * Apply GMI formula
 * Some care must be taken with variables not at their bounds (basic or free).
 * Depending on the case, it might be impossible to generate the cut!
 */

bool GomorySeparator::applyGMI(Model& m)
{
	DOMINIQS_DEBUG_LOG( applyChrono.start() );
	double f0 = fractionalPart(beta);
	beta = f0;
	double ratio = f0 / (1.0 - f0);
	// derive >= fractional cut
	// handle structural variables
	for (int j: xSupport)
	{
		// complement phase
		if (isNull(xCoef[j], epsBigVars))
		{
			xCoef[j] = 0.0;
			continue;
		}
		if (xStatus[j] == 'R')
		{
			// if we have a free continuous var with non-zero coef we cannot generate a GMI
			if (m.xType[j] == 'C') return false;
			// if we have a free integer var with non-integer coef we cannot generate a GMI
			if (!isInteger(xCoef[j], epsBigVars)) return false;
			// xCoef[j] = 0.0;
		}
		else if (xStatus[j] == 'F')
		{
			// fixed variable => simplify
			xCoef[j] = 0.0; // because we already shifted the bounds!
			continue;
		}
		else if (xStatus[j] == 'U')
		{
			// complement
			xCoef[j] = -xCoef[j];
		}

		// GMI formula phase
		double fj = fractionalPart(xCoef[j], epsBigVars);
		if (m.xType[j] != 'C')
		{
			// integral
			if (fj > f0) xCoef[j] = (1.0 - fj) * ratio;
			else xCoef[j] = fj;
		}
		else
		{
			// continuous
			if (isNegative(xCoef[j], epsBigVars)) xCoef[j] = -xCoef[j] * ratio;
		}

		// undo complement
		if (xStatus[j] == 'L') beta += (m.xLb[j] * xCoef[j]);
		else if (xStatus[j] == 'U')
		{
			beta -= (m.xUb[j] * xCoef[j]);
			xCoef[j] = -xCoef[j];
		}
	}
	// handle artificial variables
	for(int i: sSupport)
	{
		double fi = fractionalPart(sCoef[i], epsBigVars);
		if (m.rows[i]->slackType != 'C')
		{
			// integral
			if (fi > f0) sCoef[i] = (1.0 - fi) * ratio;
			else sCoef[i] = fi;
		}
		else
		{
			// continuous
			if (isNegative(sCoef[i], epsBigVars)) sCoef[i] = -sCoef[i] * ratio;
		}
	}
	DOMINIQS_DEBUG_LOG( applyChrono.stop() );
	return true;
}

/**
 * Back substitute slacks, in order to have a cut in the structural variables space
 */

void GomorySeparator::backSubstitute(Model& m)
{
	DOMINIQS_DEBUG_LOG( backChrono.start() );
	for (int i: sSupport)
	{
		DOMINIQS_ASSERT( m.sense[i] != 'E' );
		double lambda = sCoef[i];
		if (m.sense[i] == 'L') lambda = -lambda;
		accumulate(&xCoef[0], m.rows[i]->row, lambda);
		beta += lambda * m.rhs[i];
		// sCoef[i] = 0.0;
	}
	DOMINIQS_DEBUG_LOG( backChrono.stop() );
}

void GomorySeparator::printRow(const Model& m)
{
	// print tableau row
	for (int j: xSupport)
	{
		if (isNotNull(xCoef[j])) std::cout << xCoef[j] << " " << m.xNames[j] << " ";
	}
	for (int i: sSupport)
	{
		if (isNotNull(sCoef[i])) std::cout << sCoef[i] << " " << m.rows[i]->name << " ";
	}
	std::cout << " == " << beta << std::endl;
}

// low level

/**
 * Analyze current LP solution to decide which variables are to be complemented
 * and calculates shifted rhs
 */

void GomorySeparator::setX(dominiqs::Model& m, const std::vector<double>& x)
{
	xStar = x;
	badIdx.clear();
	badVal.clear();
	badType.clear();
	numBadX = 0;
	numBad = 0;
	// get current base
	ws.read(m.env, m.lp);
	// handle strucutural variable
	xStatus.resize(m.numVars);
	for (unsigned int j = 0; j < m.numVars; j++)
	{
		if ((m.xSimplexType[j] == 'B') || (m.xSimplexType[j] == 'H'))
		{
			double distanceFromLb = x[j] - m.xLb[j] + epsSmallVars;
			double distanceFromUb = m.xUb[j] - x[j] + epsSmallVars;
			if (distanceFromLb <= distanceFromUb)
			{
				// treat as non-negative variable
				xStatus[j] = 'L';
			}
			else
			{
				// treat as non-positive variable
				xStatus[j] = 'U';
			}
		}
		else if (m.xSimplexType[j] == 'P')
		{
			// non-negative variable
			xStatus[j] = 'L';
		}
		else if (m.xSimplexType[j] == 'N')
		{
			// non-positive variable
			xStatus[j] = 'U';
		}
		else if (m.xSimplexType[j] == 'R')
		{
			// free variable
			xStatus[j] = 'R';
		}
		else if (m.xSimplexType[j] == 'F')
		{
			// fixed variable
			xStatus[j] = 'F';
		}

		// is this a bad var?
		if (ws.cstat[j] != CPX_BASIC)
		{
			if ((xStatus[j] == 'L') && greaterThan(xStar[j], m.xLb[j]))
			{
				badIdx.push_back(j);
				badVal.push_back(xStar[j] - m.xLb[j]);
				badType.push_back(m.xType[j]);
			}
			if ((xStatus[j] == 'U') && lessThan(xStar[j], m.xUb[j]))
			{
				badIdx.push_back(j);
				badVal.push_back(m.xUb[j] - xStar[j]);
				badType.push_back(m.xType[j]);
			}
			if (xStatus[j] == 'R')
			{
				badIdx.push_back(j);
				badVal.push_back(xStar[j]);
				badType.push_back(m.xType[j]);
			}
		}
	}
	numBadX = badIdx.size();
	// shifted rhs & sStar
	shiftedRhs = m.rhs;
	sStar.clear();
	for (unsigned int i = 0; i < m.numRows; i++)
	{
		const SparseVector& row = m.rows[i]->row;
		const int* idx = row.idx();
		const double* coef = row.coef();
		for (unsigned int k = 0; k < row.size(); k++)
		{
			int j = idx[k];
			double v = coef[k];
			if (xStatus[j] == 'L') shiftedRhs[i] -= (v * m.xLb[j]);
			else if (xStatus[j] == 'U') shiftedRhs[i] -= (v * m.xUb[j]);
			else if (xStatus[j] == 'F') shiftedRhs[i] -= (v * m.xLb[j]);
		}
		if (m.sense[i] != 'E')
		{
			double slack = dotProduct(row, &xStar[0]) - m.rhs[i];
			if (m.sense[i] == 'L') slack = -slack;
			/*if (isNegative(slack))
			{
				std::cout << m.rows[i]->name << " has negative slack: " << std::setprecision(10) << slack << std::endl;
			}
			DOMINIQS_ASSERT( !isNegative(slack) );*/
			sStar.push_back(slack);
			if (isPositive(slack, epsSmallVars))
			{
				badIdx.push_back(i);
				badVal.push_back(slack);
				badType.push_back(m.rows[i]->slackType);
			}
		}
	}
	numBad = badIdx.size();
	badRatio = (double)numBad / (m.numVars + m.numSlacks);
	DOMINIQS_ASSERT_EQUAL( sStar.size(), m.numSlacks );
}

dominiqs::CutPtr GomorySeparator::separateOneGMI(dominiqs::Model& m, const dominiqs::numarray<double>& multipliers, bool& violated)
{
	violated = false;
	DOMINIQS_ASSERT( multipliers.size() >= m.numRows );
	if (!calculateTableauRow(m, multipliers)) return CutPtr();
	if (!applyGMI(m)) return CutPtr();
	backSubstitute(m);
	numTried++;
	CutPtr r(new Cut);
	r->row.reserve(xSupport.size());
	int maxSize = std::max(int(maxSupport * m.numVars), 100);
	double violation = beta;
	for (int j: xSupport)
	{
		if (isNotNull(xCoef[j], epsBigVars))
		{
			r->row.push_unsafe(j, xCoef[j]);
			violation -= (xStar[j] * xCoef[j]);
			if (r->row.size() > (unsigned int)maxSize) return CutPtr();
		}
	}
	if (onlyViolated && (violation < minViolation)) return CutPtr();
	if (r->row.size() == 0) return CutPtr();
	r->sense = 'G';
	r->rhs = beta;
	r->slackType = 'C';
	r->removable = true;
	r->rank = rank;
	r->digest();
	if (r->dynamism() > maxDynamism)
	{
		numBadDynamism++;
		return CutPtr();
	}
	r->name = (boost::format("gmic_%1%") % cutCounter++).str();
#ifdef DOMINIQS_TRACE_CUTS
	r->mult = multipliers;
	r->complemented.clear();
	for (int j: xSupport)
	{
		if (xStatus[j] == 'U') r->complemented.push_back(j);
	}
#endif // DOMINIQS_TRACE_CUTS
	violated = (violation >= minViolation);
	return r;
}

dominiqs::CutPtr GomorySeparator::separateOneGMI(dominiqs::Model& m, int rowIdx, bool& violated)
{
	DOMINIQS_ASSERT( rowIdx >= 0 && rowIdx < (int)m.numRows );
	// get multipliers
	mult.resize(m.numRows);
	CHECKED_CPX_CALL( CPXbinvrow, m.env, m.lp, rowIdx, mult.c_ptr() );
	return separateOneGMI(m, mult, violated);
}

// high level

bool GomorySeparator::separateGMI(Model& m, const std::vector<double>& x, CutPool& pool)
{
	CutList cuts;
	bool foundViolated = separateGMI(m, x, cuts);
	for (CutPtr c: cuts) pool.push(c);
	return foundViolated;
}

bool GomorySeparator::separateGMI(dominiqs::Model& m, const std::vector<double>& x, dominiqs::CutList& cuts)
{
	watch.start();
	DOMINIQS_DEBUG_LOG( initChrono.start() );
	if (forceRefactor) refactor(m);
	dominiqs::numarray<int> bhead(m.numRows);
	dominiqs::numarray<double> bval(m.numRows);
	CHECKED_CPX_CALL( CPXgetbhead, m.env, m.lp, &bhead[0], &bval[0] );
	for (unsigned int i = 0; i < m.numRows; i++)
	{
		if ((bhead[i] < 0) || (m.xType[bhead[i]] == 'C')) bval[i] = 0;
		else bval[i] = integralityViolation(bval[i], epsSmallVars);
	}
	dominiqs::numarray<int> perm(m.numRows);
	permShellSort(bval.c_ptr(), perm.c_ptr(), m.numRows, std::greater<double>());
	setX(m, x);
	DOMINIQS_DEBUG_LOG( initChrono.stop() );
	bool foundViolated = false;
	for (unsigned int i = 0; i < m.numRows; i++)
	{
		if (bval[perm[i]] > minViolation)
		{
			bool thisViolated = false;
			CutPtr newCut = separateOneGMI(m, perm[i], thisViolated);
			foundViolated |= thisViolated;
			if (newCut) cuts.push_back(newCut);
		}
		else break;
		if ((int)cuts.size() > maxGMI) break;
	}
	watch.stop();
	return foundViolated;
}

void GomorySeparator::logStats()
{
	gLog().setConsoleEcho(false);
	gLog().startSection("stats", "name", name);
	gLog().logItem("initTime", initChrono.getTotal());
	gLog().logItem("tabTime", tabRowChrono.getTotal());
	gLog().logItem("applyTime", applyChrono.getTotal());
	gLog().logItem("backTime", backChrono.getTotal());
	gLog().logItem("numImprKCuts", numImprKCuts);
	gLog().logItem("numBadDynamism", numBadDynamism);
	gLog().logItem("numTried", numTried);
	gLog().endSection();
	gLog().setConsoleEcho(true);
}

void GomorySeparator::refactor(dominiqs::Model& m)
{
	// reload basis (should trigger a refactor)
	ws.read(m.env, m.lp);
	ws.write(m.env, m.lp);
}

} // namespace dominiqs
