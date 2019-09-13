/**
 * @file cpxutils.cpp
 * @brief Cplex API Utilities
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2007-2009
 */

#include <iostream>

#include <utils/asserter.h>
#include <utils/maths.h>
#include <utils/chrono.h>
#include <utils/floats.h>

#include "cpxutils.h"

using namespace std;

namespace dominiqs {

void getVarNames(CEnv env, CProb lp, std::vector<std::string>& names, int begin, int end)
{
	int n = CPXgetnumcols(env, lp);
	if (end == -1) end = n - 1;
	DOMINIQS_ASSERT( begin <= end );
	int count = end - begin + 1;
	vector<char> buffer;
	vector<char*> cnames;
	cnames.resize(count, 0);
	int surplus;
	CPXgetcolname(env, lp, &cnames[0], 0, 0, &surplus, begin, end);
	if (surplus)
	{
		buffer.resize(-surplus);
		CHECKED_CPX_CALL( CPXgetcolname, env, lp, &cnames[0], &buffer[0], buffer.size(), &surplus, begin, end );
		for (int i = 0; i < count; i++) names.push_back(string(cnames[i]));
	}
	else
	{
		// no names
		for (int i = 0; i < count; i++) names.push_back("");
	}
}

void getConstrNames(CEnv env, CProb lp, std::vector<std::string>& names, int begin, int end)
{
	int m = CPXgetnumrows(env, lp);
	if (end == -1) end = m - 1;
	if (begin > end) return; // problem with no rows (may happen!)
	int count = end - begin + 1;
	vector<char> buffer;
	vector<char*> rnames;
	rnames.resize(count, 0);
	int surplus;
	CPXgetrowname(env, lp, &rnames[0], 0, 0, &surplus, begin, end);
	if (surplus)
	{
		buffer.resize(-surplus);
		CHECKED_CPX_CALL( CPXgetrowname, env, lp, &rnames[0], &buffer[0], buffer.size(), &surplus, begin, end );
		for (int i = 0; i < count; i++) names.push_back(string(rnames[i]));
	}
	else
	{
		// no names
		for (int i = 0; i < count; i++) names.push_back("");
	}
}

int getRowSupportLength(CEnv env, CProb lp, int row_idx)
{
	int tmp = 0;
	int size;
	CPXgetrows(env, lp, &tmp, &tmp, 0, 0, 0, &size, row_idx, row_idx);
	return -size;
}

int getColSupportLength(CEnv env, CProb lp, int col_idx)
{
	int tmp = 0;
	int size;
	CPXgetcols(env, lp, &tmp, &tmp, 0, 0, 0, &size, col_idx, col_idx);
	return -size;
}

void addCut(CEnv env, Prob lp, const std::string& name, const int* idx, const double* val, int cnt, char sense, double rhs)
{
	int matbeg = 0;
	char* rname = (char*)(name.c_str());
	CHECKED_CPX_CALL( CPXaddrows, env, lp, 0, 1, cnt, &rhs, &sense, &matbeg, idx, val, 0, &rname );
}

void addCut(CEnv env, Prob lp, const std::string& name, const SparseVector& row, char sense, double rhs)
{
	addCut(env, lp, name, row.idx(), row.coef(), row.size(), sense, rhs);
}

void addUserCut(CEnv env, Prob lp, const std::string& name, const SparseVector& row, char sense, double rhs)
{
	int matbeg = 0;
	char* rname = (char*)(name.c_str());
	CHECKED_CPX_CALL( CPXaddusercuts, env, lp, 1, row.size(), &rhs, &sense, &matbeg, row.idx(), row.coef(), &rname );
}

void addEmptyVar(CEnv env, Prob lp, const std::string& name, char ctype, double lb, double ub, double obj)
{
	char* cname = (char*)(name.c_str());
	CHECKED_CPX_CALL( CPXnewcols, env, lp, 1, &obj, &lb, &ub, &ctype, &cname );
}

void addVar(CEnv env, Prob lp, const std::string& name, const int* idx, const double* val, int cnt, char ctype, double lb, double ub, double obj)
{
	int matbeg = 0;
	char* cname = (char*)(name.c_str());
	if (cnt > 0)
	{
		CHECKED_CPX_CALL( CPXaddcols, env, lp, 1, cnt, &obj, &matbeg, idx, val, &lb, &ub, &cname );
		if (ctype != 'C')
		{
			int last = CPXgetnumcols(env, lp) - 1;
			CHECKED_CPX_CALL( CPXchgctype, env, lp, 1, &last, &ctype );
		}
	}
	else CHECKED_CPX_CALL( CPXnewcols, env, lp, 1, &obj, &lb, &ub, &ctype, &cname );
}

void addVar(CEnv env, Prob lp, const std::string& name, const SparseVector& col, char ctype, double lb, double ub, double obj)
{
	addVar(env, lp, name, col.idx(), col.coef(), col.size(), ctype, lb, ub, obj);
}

void getCut(CEnv env, CProb lp, int row_idx, SparseVector& row, char& sense, double& rhs)
{
	// get row nz
	int tmp = 0;
	int size;
	CPXgetrows(env, lp, &tmp, &tmp, 0, 0, 0, &size, row_idx, row_idx);
	// get coef
	size = -size;
	if (size)
	{
		row.resize(size);
		CHECKED_CPX_CALL( CPXgetrows, env, lp, &tmp, &tmp, row.idx(), row.coef(), size, &tmp, row_idx, row_idx );
	}
	else
	{
		row.clear();
	}
	// here to correctly handle empty constraints
	// get rhs
	CHECKED_CPX_CALL( CPXgetrhs, env, lp, &rhs, row_idx, row_idx );
	// get sense
	CHECKED_CPX_CALL( CPXgetsense, env, lp, &sense, row_idx, row_idx );
}

void getCut(CEnv env, CProb lp, const std::string& row_name, SparseVector& row, char& sense, double& rhs)
{
	int row_idx;
	CHECKED_CPX_CALL( CPXgetrowindex, env, lp, row_name.c_str(), &row_idx );
	getCut(env, lp, row_idx, row, sense, rhs);
}

void getVar(CEnv env, CProb lp, int col_idx, SparseVector& col, char& type, double& lb, double& ub, double& obj)
{
	// get col nz
	int tmp = 0;
	int size;
	CPXgetcols(env, lp, &tmp, &tmp, 0, 0, 0, &size, col_idx, col_idx);
	// get coef
	size = -size;
	if (size)
	{
		col.resize(size);
		CHECKED_CPX_CALL( CPXgetcols, env, lp, &tmp, &tmp, col.idx(), col.coef(), size, &tmp, col_idx, col_idx );
	}
	else
	{
		col.clear();
	}
	// here to correctly handle empty vars
	// get bounds
	CHECKED_CPX_CALL( CPXgetlb, env, lp, &lb, col_idx, col_idx );
	CHECKED_CPX_CALL( CPXgetub, env, lp, &ub, col_idx, col_idx );
	// get obj
	CHECKED_CPX_CALL( CPXgetobj, env, lp, &obj, col_idx, col_idx );
	// get type
	status = CPXgetctype(env, lp, &type, col_idx, col_idx);
	if (status) type = 'C'; // cannot call CPXgetctype on an LP
}

void getVar(CEnv env, CProb lp, const std::string& col_name, SparseVector& col, char& type, double& lb, double& ub, double& obj)
{
	int col_idx;
	CHECKED_CPX_CALL( CPXgetcolindex, env, lp, col_name.c_str(), &col_idx );
	getVar(env, lp, col_idx, col, type, lb, ub, obj);
}

void delCut(CEnv env, Prob lp, const std::string& row_name)
{
	int row_idx;
	CHECKED_CPX_CALL( CPXgetrowindex, env, lp, row_name.c_str(), &row_idx );
	CHECKED_CPX_CALL( CPXdelrows, env, lp, row_idx, row_idx );
}

void delCut(CEnv env, Prob lp, int row_idx)
{
	CHECKED_CPX_CALL( CPXdelrows, env, lp, row_idx, row_idx );
}

void delVar(CEnv env, Prob lp, int col_idx)
{
	CHECKED_CPX_CALL( CPXdelcols, env, lp, col_idx, col_idx );
}

void addMIPStart(CEnv env, Prob lp, const double* sol, int cnt)
{
	int beg = 0;
	std::vector<int> idx(cnt);
	dominiqs::iota(idx.begin(), idx.end(), 0);
	CHECKED_CPX_CALL( CPXaddmipstarts, env, lp, 1, cnt, &beg, &idx[0], sol, 0, 0 );
}

//**********
// WarmStart
//**********

void WarmStart::read(CEnv env, CProb lp)
{
	int n = CPXgetnumcols(env, lp);
	int m = CPXgetnumrows(env, lp);
	cstat.resize(n);
	rstat.resize(m);
	CHECKED_CPX_CALL( CPXgetbase, env, lp, &cstat[0], &rstat[0] );
}

void WarmStart::write(Env env, Prob lp)
{
	DOMINIQS_ASSERT( CPXgetnumcols(env, lp) == (int)cstat.size() );
	DOMINIQS_ASSERT( CPXgetnumrows(env, lp) == (int)rstat.size() );
	CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_ADVIND, 1 );
	CHECKED_CPX_CALL( CPXcopybase, env, lp, &cstat[0], &rstat[0] );
	int itLim = 0;
	CHECKED_CPX_CALL( CPXgetintparam, env, CPX_PARAM_ITLIM, &itLim );
	CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_ITLIM, 0 );
	CHECKED_CPX_CALL( CPXlpopt, env, lp );
	CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_ITLIM, itLim );
}

//*****************
// OptimalFaceFixer
//*****************

void OptimalFaceFixer::init(CEnv env, Prob lp)
{
	int n = CPXgetnumcols(env, lp);
	int m = CPXgetnumrows(env, lp);
	// original problem data (used for restore)
	sense.resize(m);
	CHECKED_CPX_CALL( CPXgetsense, env, lp, &sense[0], 0, m - 1 );
	lb.resize(n);
	ub.resize(n);
	CHECKED_CPX_CALL( CPXgetlb, env, lp, &lb[0], 0, n - 1 );
	CHECKED_CPX_CALL( CPXgetub, env, lp, &ub[0], 0, n - 1 );
	obj.resize(n);
	x.resize(n);
	red.resize(n);
	dual.resize(m);
	cstat.resize(n);
	rstat.resize(m);
	rowIndices.resize(m);
	iota(rowIndices.begin(), rowIndices.end(), 0);
	colIndices.resize(n);
	iota(colIndices.begin(), colIndices.end(), 0);
}

void OptimalFaceFixer::exec(CEnv env, Prob lp)
{
	int n = CPXgetnumcols(env, lp);
	int m = CPXgetnumrows(env, lp);
	// get solution
	CHECKED_CPX_CALL( CPXgetobj, env, lp, &obj[0], 0, n - 1 );
	CHECKED_CPX_CALL( CPXgetobjval, env, lp, &origObjValue );
	CHECKED_CPX_CALL( CPXgetx, env, lp, &x[0], 0, n - 1 );
	CHECKED_CPX_CALL( CPXgetdj, env, lp, &red[0], 0, n - 1 );
	CHECKED_CPX_CALL( CPXgetpi, env, lp, &dual[0], 0, m - 1 );
	CHECKED_CPX_CALL( CPXgetbase, env, lp, &cstat[0], &rstat[0] );
	// fix on optimal face
	char change = 'B';
	char newSense = 'E';
	for (int i = 0; i < n; i++)
	{
		if ((cstat[i] != CPX_BASIC) && different(lb[i], ub[i]) && isNotNull(red[i]))
		{
			CHECKED_CPX_CALL( CPXchgbds, env, lp, 1, &i, &change, &x[i] );
		}
	}
	for (int i = 0; i < m; i++)
	{
		if ((rstat[i] != CPX_BASIC) && (sense[i] != 'E') && isNotNull(dual[i]))
		{
			CHECKED_CPX_CALL( CPXchgsense, env, lp, 1, &i, &newSense );
		}
	}
}

void OptimalFaceFixer::undo(CEnv env, Prob lp)
{
	int n = CPXgetnumcols(env, lp);
	int m = CPXgetnumrows(env, lp);
	// get solution
	CHECKED_CPX_CALL( CPXgetx, env, lp, &x[0], 0, n - 1 );
	double lastObjValue = dotProduct(&x[0], &obj[0], n);
	if (!relEqual(origObjValue, lastObjValue)) std::cout << origObjValue << " != " << lastObjValue << std::endl;
	DOMINIQS_ASSERT( relEqual(origObjValue, lastObjValue) );
	// restore
	CHECKED_CPX_CALL( CPXchgsense, env, lp, m, &rowIndices[0], &sense[0] );
	std::vector<char> bds(n, 'L');
	CHECKED_CPX_CALL( CPXchgbds, env, lp, n, &colIndices[0], &bds[0], &lb[0] );
	fill(bds.begin(), bds.end(), 'U');
	CHECKED_CPX_CALL( CPXchgbds, env, lp, n, &colIndices[0], &bds[0], &ub[0] );
}

//*********
// ProbInfo
//*********

ProbInfo::ProbInfo()
{
	clear();
}

VarPtr ProbInfo::getVarByName(const std::string& name) const
{
	for (VarPtr v: vars)
	{
		if (v->name == name) return v;
	}
	return VarPtr();
}

ConstraintPtr ProbInfo::getConstraintByName(const std::string& name) const
{
	for (ConstraintPtr c: constraints)
	{
		if (c->name == name) return c;
	}
	return ConstraintPtr();
}

void ProbInfo::push(VarPtr v)
{
	vars.push_back(v);
	numVars++;
	if (v->type == 'B') numBinVars++;
	else if (v->type == 'I') numIntVars++;
	else if (v->type == 'C') numContVars++;
}

void ProbInfo::push(ConstraintPtr c)
{
	constraints.push_back(c);
	numConstrs++;
}

void ProbInfo::clear()
{
	vars.clear();
	constraints.clear();
	numVars = 0;
	numBinVars = 0;
	numIntVars = 0;
	numContVars = 0;
	numConstrs = 0;
	numNz = 0;
	obj_sense = 0;
}

void ProbInfo::read(CEnv env, CProb lp)
{
	clear();
	// extract dims
	int rows = CPXgetnumrows(env, lp);
	int cols = CPXgetnumcols(env, lp);
	// extract names
	std::vector<std::string> row_names;
	std::vector<std::string> col_names;
	getVarNames(env, lp, col_names);
	getConstrNames(env, lp, row_names);
	// extract column info
	for (int i = 0; i < cols; i++){
		VarPtr v(new Var);
		v->name = col_names[i];
		getVar(env, lp, i, v->col, v->type, v->lb, v->ub, v->objCoef);
		push(v);
	}
	DOMINIQS_ASSERT( numVars == vars.size() );
	DOMINIQS_ASSERT( numVars == numBinVars + numIntVars + numContVars );
	// extract row info
	std::vector<char> sense(rows);
	std::vector<double> rhs(rows);
	CHECKED_CPX_CALL( CPXgetrhs, env, lp, &rhs[0], 0, rows - 1 );
	CHECKED_CPX_CALL( CPXgetsense, env, lp, &sense[0], 0, rows - 1 );
	numNz = CPXgetnumnz(env, lp);
	int nzcnt;
	int surplus;
	std::vector<int> matbeg(rows);
	std::vector<int> matind(numNz);
	std::vector<double> matval(numNz);
	CHECKED_CPX_CALL( CPXgetrows, env, lp, &nzcnt, &matbeg[0], &matind[0], &matval[0], numNz, &surplus, 0, rows - 1 );
	for (int i = 0; i < rows; i++){
		ConstraintPtr c(new Constraint);
		c->name = row_names[i];
		if (i == (rows - 1))
		{
			unsigned int cnt = matind.size() - matbeg[i];
			c->row.resize(cnt);
			std::copy(matind.begin() + matbeg[i], matind.end(), c->row.idx());
			std::copy(matval.begin() + matbeg[i], matval.end(), c->row.coef());
		}
		else
		{
			unsigned int cnt = matbeg[i+1] - matbeg[i];
			c->row.resize(cnt);
			std::copy(matind.begin() + matbeg[i], matind.begin() + matbeg[i+1], c->row.idx());
			std::copy(matval.begin() + matbeg[i], matval.begin() + matbeg[i+1], c->row.coef());
		}
		c->sense = sense[i];
		c->rhs = rhs[i];
		push(c);
	}
	DOMINIQS_ASSERT( numConstrs == constraints.size() );
	obj_sense = CPXgetobjsen(env, lp);
}

Prob ProbInfo::write(CEnv env)
{
	DECL_PROB( env, lp );
	for (VarPtr v: vars) { addEmptyVar(env, lp, v->name, v->type, v->lb, v->ub, v->objCoef); }
	for (ConstraintPtr c: constraints) { addCut(env, lp, c->name, c->row, c->sense, c->rhs); }
	return lp;
}

//*********
// Presolve
//*********

typedef int(CPXPUBLIC*BranchCbPtr)(CALLBACK_BRANCH_ARGS);

static int branchCB(CALLBACK_BRANCH_ARGS)
{
	Prob nodelp;
	CHECKED_CPX_CALL( CPXgetcallbacknodelp, xenv, cbdata, wherefrom, &nodelp );
	CpxPresolver* ud = static_cast<CpxPresolver*>(cbhandle);
	// get # cuts
	CLONE_PROB(xenv, nodelp, ud->afterRootLp);
	ud->addedCuts = CPXgetnumrows(xenv, ud->afterRootLp) - CPXgetnumrows(xenv, ud->presolvedLp);
	// copy ctype
	int n = CPXgetnumcols(xenv, ud->afterRootLp);
	DOMINIQS_ASSERT( n == CPXgetnumcols(xenv, ud->presolvedLp) );
	std::vector<char> ctype(n);
	CHECKED_CPX_CALL( CPXgetcallbackctype, xenv, cbdata, wherefrom, &ctype[0], 0, n - 1 );
	CHECKED_CPX_CALL( CPXcopyctype, xenv, ud->afterRootLp, &ctype[0] );
	// get incumbent
	int feas = 0;
	CHECKED_CPX_CALL( CPXgetcallbackinfo, xenv, cbdata, wherefrom, CPX_CALLBACK_INFO_MIP_FEAS, (void*)&feas );
	if (feas)
	{
		ud->incumbent.resize(n);
		CHECKED_CPX_CALL( CPXgetcallbackincumbent, xenv, cbdata, wherefrom, &(ud->incumbent[0]), 0, n - 1 );
		std::vector<double> obj(n);
		CHECKED_CPX_CALL( CPXgetobj, xenv, ud->afterRootLp, &obj[0], 0,  n - 1 );
		ud->incumbentValue = dotProduct(&obj[0], &(ud->incumbent[0]), n);
	}
	return -1;
}

void CpxPresolver::exec(Env env, Prob lp, const std::string& solFile)
{
	incumbent.clear();
	incumbentValue = CPX_INFBOUND;
	isOptimal = false;
	CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_REPEATPRESOLVE, 0 );
	CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_MIPCBREDLP, CPX_ON );
	if (solFile.size()) { CHECKED_CPX_CALL( CPXreadcopysol, env, lp, solFile.c_str() ); }
	// protect binary variables
	if (preserveBinary && CPXgetnumbin(env, lp))
	{
		CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_AGGIND, CPX_OFF );
		int n = CPXgetnumcols(env, lp);
		std::vector<char> ctype(n);
		CHECKED_CPX_CALL( CPXgetctype, env, lp, &ctype[0], 0, n - 1 );
		std::vector<int> binaries;
		for (int i = 0; i < n; i++) if (ctype[i] == 'B') binaries.push_back(i);
		CHECKED_CPX_CALL( CPXcopyprotected, env, lp, binaries.size(), &binaries[0] );
	}
	Chrono chrono(true);
	CHECKED_CPX_CALL( CPXpresolve, env, lp, CPX_ALG_NONE );
	chrono.stop();
	presolveTime = chrono.getTotal();
	int preStat;
	CHECKED_CPX_CALL( CPXgetprestat, env, lp, &preStat, 0, 0, 0, 0 );
	std::cout << "Presolve stat: " << preStat << std::endl;
	// get presolved problem
	if (preStat == 2) return;
	else if (preStat == 0)
	{
		objOffset = 0.0;
		CLONE_PROB(env, lp, presolvedLp);
	}
	else
	{
		CProb tmplp;
		CHECKED_CPX_CALL( CPXgetredlp, env, lp, &tmplp );
		CLONE_PROB(env, tmplp, presolvedLp);
		CHECKED_CPX_CALL( CPXgetobjoffset, env, presolvedLp, &objOffset );
	}
	// std::cout << "Presolved problem #int: " << (CPXgetnumbin(env, presolvedLp) + CPXgetnumint(env, presolvedLp)) << std::endl;
	// std::cout << "Presolved problem #vars: " << CPXgetnumcols(env, presolvedLp) << std::endl;
	// std::cout << "Presolved problem #rows: " << CPXgetnumrows(env, presolvedLp) << std::endl;
	if (rootNodeProcessing)
	{
		BranchCbPtr oldBranchCB;
		void* oldBranchCBHandle;
		CPXgetbranchcallbackfunc(env, &oldBranchCB, &oldBranchCBHandle);
		CHECKED_CPX_CALL( CPXsetbranchcallbackfunc, env, branchCB, (void*)(this) );
		if (cutEmphasis)
		{
			CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_CLIQUES, cutEmphasis );
			CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_COVERS, cutEmphasis );
			CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_FLOWCOVERS, cutEmphasis );
			CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_IMPLBD, cutEmphasis );
			CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_GUBCOVERS, cutEmphasis );
			CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_FRACCUTS, cutEmphasis );
			CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_FLOWPATHS, cutEmphasis );
			CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_MIRCUTS, cutEmphasis );
			CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_DISJCUTS, cutEmphasis );
			CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_ZEROHALFCUTS, cutEmphasis );
			// CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_MCFCUTS, cutEmphasis );
		}
		chrono.start();
		CPXmipopt(env, lp);
		int stat = CPXgetstat(env, lp);
		chrono.stop();
		presolveTime = chrono.getTotal();
		CHECKED_CPX_CALL( CPXsetbranchcallbackfunc, env, oldBranchCB, oldBranchCBHandle );
		if ((stat == CPXMIP_OPTIMAL) || (stat == CPXMIP_OPTIMAL_TOL))
		{
			// problem solved to optimality
			int n = CPXgetnumcols(env, lp);
			incumbent.resize(n);
			CHECKED_CPX_CALL( CPXgetx, env, lp, &incumbent[0], 0, n - 1 );
			CHECKED_CPX_CALL( CPXgetobjval, env, lp, &incumbentValue );
			isOptimal = true;
		}
		else
		{
			DOMINIQS_ASSERT( afterRootLp );
			FREE_PROB( env, presolvedLp );
			presolvedLp = afterRootLp;
			afterRootLp = 0;
		}
	}
	// std::cout << "Presolved problem after root processing #int: " << (CPXgetnumbin(env, presolvedLp) + CPXgetnumint(env, presolvedLp)) << std::endl;
	// std::cout << "Presolved problem after root processing #vars: " << CPXgetnumcols(env, presolvedLp) << std::endl;
	// std::cout << "Presolved problem after root processing #rows: " << CPXgetnumrows(env, presolvedLp) << std::endl;
}

} // namespace dominiqs

