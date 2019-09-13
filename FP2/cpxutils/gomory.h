/**
 * @file gomory.h
 * @brief Gomory cuts separators
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * Copyright 2009 Domenico Salvagnin
 */

#ifndef GOMORY_H
#define GOMORY_H

#include <list>
#include <iosfwd>
#include <boost/shared_ptr.hpp>

#include <utils/chrono.h>
#include <utils/cutpool.h>

#include "cpxutils.h"
#include "model.h"

namespace dominiqs {

void printCut(std::ostream& out, const dominiqs::Model& m, const dominiqs::Cut& c);

class XMLConfig;

class GomorySeparator
{
public:
	GomorySeparator();
	virtual ~GomorySeparator();
	virtual void readConfig(dominiqs::XMLConfig& config, const std::string& root);
	// high level interface
	virtual bool separateGMI(dominiqs::Model& m, const std::vector<double>& x, dominiqs::CutPool& pool);
	virtual bool separateGMI(dominiqs::Model& m, const std::vector<double>& x, dominiqs::CutList& cuts);
	// low level interface
	virtual void setX(dominiqs::Model& m, const std::vector<double>& x);
	virtual dominiqs::CutPtr separateOneGMI(dominiqs::Model& m, const dominiqs::numarray<double>& multipliers, bool& violated);
	virtual dominiqs::CutPtr separateOneGMI(dominiqs::Model& m, int rowIdx, bool& violated);
	int getNumBadVars() const { return numBad; }
	// helpers
	void refactor(dominiqs::Model& m);
	dominiqs::Chrono watch;
	// params
	std::string name;
	double minViolation;
	double maxSupport;
	double maxDynamism;
	int maxRank;
	bool debugOn;
	bool forceRefactor;
	int maxGMI;
	bool onlyViolated; //< return (and hopefully generate) only violated cuts
	double onlyViolBadThr;
	// stats
	dominiqs::Chrono tabRowChrono;
	dominiqs::Chrono applyChrono;
	dominiqs::Chrono backChrono;
	dominiqs::Chrono initChrono;
	int numImprKCuts;
	int numBadDynamism;
	int numTried;
protected:
	// params
	double epsSmallVars;
	double epsBigVars;
	// data
	int rank;
	std::vector<double> shiftedRhs;
	dominiqs::numarray<double> xCoef;
	dominiqs::numarray<int> xSupport;
	dominiqs::numarray<int> xUsed;
	dominiqs::numarray<double> sCoef;
	dominiqs::numarray<int> sSupport;
	std::vector<char> xStatus; //< L = lower, U = upper (swapped), F = fixed, R = free
	dominiqs::numarray<double> mult;
	double beta;
	std::vector<double> xStar; //< point to separate
	dominiqs::numarray<double> sStar; //< slacks of the point to separate
	int cutCounter;
	dominiqs::WarmStart ws; //< basis to trigger refactor
	dominiqs::numarray<int> badIdx;
	dominiqs::numarray<double> badVal;
	dominiqs::numarray<char> badType;
	int numBadX;
	int numBad;
	double badRatio;
	// helpers
	virtual bool calculateTableauRow(dominiqs::Model& m, const dominiqs::numarray<double>& multipliers);
	virtual bool applyGMI(dominiqs::Model& m);
	virtual void backSubstitute(dominiqs::Model& m);
	virtual void logStats();
	// debug
	virtual void printRow(const dominiqs::Model& m);
};

} // namespace dominiqs

#endif /* GOMORY_H */
