/**
 * @file model.h
 * @brief Model Class
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * Copyright 2009 Domenico Salvagnin
 */

#ifndef MODEL_H
#define MODEL_H

#include <utils/cutpool.h>
#include "cpxutils.h"

namespace dominiqs {

class Model
{
public:
	Model() : bigRange(1E+4), linearEPS(1E-6), ageLimit(3), env(0), lp(0) {}
	Model(const Model& other);
	void extract(dominiqs::Env env, dominiqs::Prob lp, const std::vector<char>& cType = std::vector<char>());
	void switchToLP();
	void switchToMIP();
	void optimize();
	double getObjVal() const;
	void addVar(const std::string& name, double lb, double ub, char type = 'C', double obj = 0.0);
	void addCut(dominiqs::CutPtr row);
	int purgeCuts(const std::vector<double>& x);
	int purgeSlackCuts(const std::vector<double>& x);
	void purgeAll();
	bool isLinearFeasible(const std::vector<double>& x) const;
	bool isIntegerFeasible(const std::vector<double>& x) const;
	bool isFeasible(const std::vector<double>& x) const;
	int reducedCostFixing(double primalBound);

	// properties/options
	double bigRange;
	double linearEPS;
	int ageLimit;

	// data
	dominiqs::Env env;
	dominiqs::Prob lp;

	unsigned int numVars;
	unsigned int numBoundedVars;
	unsigned int numRows;
	unsigned int numOrigRows;
	unsigned int numSlacks;

	std::vector<double> xLb;
	std::vector<double> xUb;
	std::vector<char> xType; //< C = continuous, B = binary, I = integer
	std::vector<char> xSimplexType; //< F = fixed, P = positive, N = negative, B = bounded, H = huge bounded, R = free
	std::vector<std::string> xNames;
	std::vector<double> xObj;
	std::vector<dominiqs::SparseVectorPtr> cols; //< without cutting planes!
	std::vector<int> upLocks;
	std::vector<int> downLocks;

	std::vector<dominiqs::CutPtr> rows;
	std::vector<char> sense; // sense cache
	std::vector<double> rhs; // rhs cache
protected:
	void calculateSlackType(dominiqs::CutPtr r);
	void computeLocks();
};

} // namespace dominiqs

#endif /* MODEL_H */
