/**
 * @file cpxutils.h
 * @brief Cplex API Utilities
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2007-2012
 */

#ifndef CPXUTILS_H
#define CPXUTILS_H

#include <boost/shared_ptr.hpp>
#include <ilcplex/cplex.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>

#include <utils/maths.h>
#include <utils/numarray.h>
#include <utils/cutpool.h>

#include "cpxmacro.h"

namespace dominiqs {

/**
 * typedefs of basic Callable Library entities,
 * i.e. environment (Env) and problem pointers (Prob).
 */

typedef CPXENVptr Env;
typedef CPXCENVptr CEnv;
typedef CPXLPptr Prob;
typedef CPXCLPptr CProb;

/**
 * Get var names as a vector of strings
 */
void getVarNames(CEnv env, CProb lp, std::vector<std::string>& names, int begin = 0, int end = -1);

/**
 * Get constraints names as a vector of strings
 */
void getConstrNames(CEnv env, CProb lp, std::vector<std::string>& names, int begin = 0, int end = -1);

/**
 * Get Row/Col Support Length (# of coefficents != 0)
 */

int getRowSupportLength(CEnv env, CProb lp, int row_idx);
int getColSupportLength(CEnv env, CProb lp, int col_idx);

/**
 * Add A Single Cut (Row)
 */
void addCut(CEnv env, Prob lp, const std::string& name, const int* idx, const double* val, int cnt, char sense, double rhs);
void addCut(CEnv env, Prob lp, const std::string& name, const SparseVector& row, char sense, double rhs);
void addUserCut(CEnv env, Prob lp, const std::string& name, const SparseVector& row, char sense, double rhs);

/**
 * Add A Single Var (Column)
 */
void addEmptyVar(CEnv env, Prob lp, const std::string& name, char ctype, double lb, double ub, double obj);
void addVar(CEnv env, Prob lp, const std::string& name, const int* idx, const double* val, int cnt, char ctype, double lb, double ub, double obj);
void addVar(CEnv env, Prob lp, const std::string& name, const SparseVector& col, char ctype, double lb, double ub, double obj);

/**
 * Get a Single Constraint (Row)
 * Available in two version: get row by idx and by name
 */

void getCut(CEnv env, CProb lp, int row_idx, SparseVector& row, char& sense, double& rhs);
void getCut(CEnv env, CProb lp, const std::string& row_name, SparseVector& row, char& sense, double& rhs);

/**
 * Get a Single Variable (Column)
 * Available in two version: get col by idx and by name
 */

void getVar(CEnv env, CProb lp, int col_idx, SparseVector& col, char& type, double& lb, double& ub, double& obj);
void getVar(CEnv env, CProb lp, const std::string& col_name, SparseVector& col, char& type, double& lb, double& ub, double& obj);

/**
 * Delete a Single Constraint (Row)
 */

void delCut(CEnv env, Prob lp, const std::string& row_name);
void delCut(CEnv env, Prob lp, int row_idx);

/**
 * Delete a Single Var (Column)
 */

void delVar(CEnv env, Prob lp, const std::string& col_name);
void delVar(CEnv env, Prob lp, int col_idx);

/**
 * Add A Single Cut (Row)
 */
void addMIPStart(CEnv env, Prob lp, const double* sol, int cnt);

/**
 * WarmStart management class
 */

class WarmStart
{
public:
	void read(CEnv env, CProb lp);
	void write(Env env, Prob lp);
	// data
	dominiqs::numarray<int> cstat;
	dominiqs::numarray<int> rstat;
};

/**
 * OptimalFaceFixer class
 * This class provides an (almost) efficient method to fix
 * on the optimal face of a polyhedron (and back!)
 */

class OptimalFaceFixer
{
public:
	void init(dominiqs::CEnv env, dominiqs::Prob lp);
	void exec(dominiqs::CEnv env, dominiqs::Prob lp);
	void undo(dominiqs::CEnv env, dominiqs::Prob lp);
protected:
	// common
	dominiqs::numarray<double> lb;
	dominiqs::numarray<double> ub;
	dominiqs::numarray<char> sense;
	dominiqs::numarray<double> obj;
	double origObjValue;
	dominiqs::numarray<int> rowIndices;
	dominiqs::numarray<int> colIndices;
	// dynamic
	dominiqs::numarray<double> red;
	dominiqs::numarray<double> x;
	dominiqs::numarray<double> dual;
	dominiqs::numarray<int> cstat;
	dominiqs::numarray<int> rstat;
};

/**
 * Lex Opt
 */

void lexMin(Env env, Prob lp, bool isMin, bool skipOrigObj, int* rank, int rankCnt);

/**
 * Var struct (column)
 */

class Var
{
public:
	/** @name Data */
	//@{
	std::string name;
	char type; /**< variable type */
	double lb; /**< lower bound \f$ l_j \f$ */
	double ub; /**< upper bound \f$ u_j \f$ */
	SparseVector col;
	double objCoef; /**< objective coefficient \f$ c_j \f$*/
	//@}
	Var* clone() const { return new Var(*this); }
};

typedef boost::shared_ptr<Var> VarPtr;

/**
 * ProbInfo class
 * This class provides efficient access to all the problem data
 * in both row and column major
 */

class ProbInfo
{
public:
	ProbInfo();
	void clear();
	/** @name Data */
	//@{
	// Dimensions
	unsigned int numVars;
	unsigned int numBinVars;
	unsigned int numIntVars;
	unsigned int numContVars;
	unsigned int numConstrs;
	unsigned int numNz;
	// Data
	std::vector<ConstraintPtr> constraints;
	std::vector<VarPtr> vars;
	int obj_sense;
	//@}
	void read(CEnv env, CProb lp);
	Prob write(CEnv env);
	// get object by name
	VarPtr getVarByName(const std::string& name) const;
	ConstraintPtr getConstraintByName(const std::string& name) const;
	// push stuff
	void push(VarPtr v);
	void push(ConstraintPtr c);
};

/**
 * Let Cplex generate a presolved problem (and perform root node processing if cuts are needed)
 * @param env: cplex environment
 * @param lp: problem to presolve
 * @param objOffset: get objective offset due to presolve
 * @param preservebinary: disable binary variables aggregation
 * @param rootNodeProcessing: do full root node processing
 */

class CpxPresolver
{
public:
	CpxPresolver()
		: preserveBinary(false), rootNodeProcessing(false), cutEmphasis(0), presolvedLp(0), afterRootLp(0), objOffset(0.0),
		addedCuts(0), incumbentValue(1e20), presolveTime(0.0), isOptimal(false) {}
	void exec(Env env, Prob lp, const std::string& solFile = "");
	// parameters
	bool preserveBinary;
	bool rootNodeProcessing;
	int cutEmphasis;
	// data
	Prob presolvedLp;
	Prob afterRootLp;
	double objOffset;
	int addedCuts;
	std::vector<double> incumbent;
	double incumbentValue;
	double presolveTime;
	bool isOptimal;
};

/**
 * Let Cplex perform its Branch & Bound on a given problem
 * @param env: cplex environment
 * @param lp: problem to solve
 * @param objOffset: objective offset due to presolve
 * @param pool: cuts to add to Cplex pool
 * @param x: incumbent if available
 * @param timeLimit: time limit
 */

class BBLimits
{
public:
	BBLimits() : timeLimit(3600.0), nodeLimit(2100000000), memLimit(4e9), enablePresolve(true), enableCuts(true) {}
	double timeLimit;
	int nodeLimit;
	double memLimit;
	bool enablePresolve;
	bool enableCuts;
};

void cpxBB(Env env, Prob lp, double objOffset, dominiqs::CutPool& pool, std::vector<double>& x, BBLimits& limits);

} // namespace dominiqs

#endif /* CPXUTILS_H */
