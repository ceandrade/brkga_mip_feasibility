/**
 * @file cpxbb.cpp
 * @brief Simple Cplex Branch & Bound
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * Copyright 2010 Domenico Salvagnin
 */

#include "cpxutils.h"

#include <utils/logger.h>
#include <utils/maths.h>

#include <algorithm>

namespace dominiqs {

static int infoCB(CEnv env, void* cbdata, int wherefrom, void* cbhandle)
{
	static int count = 0;
	count++;
	if ((count % 100) == 0)
	{
		int nodeCount;
		double primalBound;
		double dualBound;
		double gap;
		CHECKED_CPX_CALL( CPXgetcallbackinfo, env, cbdata, wherefrom, CPX_CALLBACK_INFO_NODE_COUNT, (void*)(&nodeCount) );
		CHECKED_CPX_CALL( CPXgetcallbackinfo, env, cbdata, wherefrom, CPX_CALLBACK_INFO_BEST_INTEGER, (void*)(&primalBound) );
		CHECKED_CPX_CALL( CPXgetcallbackinfo, env, cbdata, wherefrom, CPX_CALLBACK_INFO_BEST_REMAINING, (void*)(&dualBound) );
		CHECKED_CPX_CALL( CPXgetcallbackinfo, env, cbdata, wherefrom, CPX_CALLBACK_INFO_MIP_REL_GAP, (void*)(&gap) );
		std::cout << boost::format("%1%\t%2%\t%3%\t%4%") % nodeCount % primalBound % dualBound % (gap * 100) << std::endl;
		if ((count % 1000) == 0)
		{
			GlobalAutoSection stamp("stamp");
			gLog().logItem("nodes", nodeCount);
			gLog().logItem("primalBound", primalBound);
			gLog().logItem("dualBound", dualBound);
			gLog().logItem("gap", gap*100);
			stamp.close();
			gLog().flush();
		}
	}
	return 0;
}

void cpxBB(Env env, Prob lp, double objOffset, dominiqs::CutPool& pool, std::vector<double>& x, BBLimits& limits)
{
	// set mip start if available
	if (x.size())
	{
		int n = CPXgetnumcols(env, lp);
		DOMINIQS_ASSERT( n == (int)x.size() );
		int beg = 0;
		std::vector<int> colIdx(n);
		dominiqs::iota(colIdx.begin(), colIdx.end(), 0);
		CHECKED_CPX_CALL( CPXaddmipstarts, env, lp, 1, n, &beg, &colIdx[0], &x[0], 0, 0 );
	}
	// add cuts as user cuts
	if (pool.size() && limits.enableCuts)
	{
		CutPool::const_iterator itr = pool.begin();
		CutPool::const_iterator end = pool.end();
		int nzcnt = 0;
		while (itr != end) nzcnt += (*itr++)->row.size();
		int ncuts = pool.size();
		std::vector<double> rhs(ncuts);
		std::vector<char> sense(ncuts);
		std::vector<int> matbeg(ncuts);
		std::vector<int> matind(nzcnt);
		std::vector<double> matval(nzcnt);
		int idx = 0;
		int coefCnt = 0;
		itr = pool.begin();
		while (itr != end)
		{
			CutPtr c = *itr++;
			rhs[idx] = c->rhs;
			sense[idx] = c->sense;
			matbeg[idx] = coefCnt;
			std::copy(c->row.idx(), c->row.idx() + c->row.size(), &matind[coefCnt]);
			std::copy(c->row.coef(), c->row.coef() + c->row.size(), &matval[coefCnt]);
			coefCnt += c->row.size();
			++idx;
		}
		DOMINIQS_ASSERT( coefCnt == nzcnt );
		CHECKED_CPX_CALL( CPXaddusercuts, env, lp, ncuts, nzcnt, &rhs[0], &sense[0], &matbeg[0], &matind[0], &matval[0], 0 );
	}
	// set limits and options
	CHECKED_CPX_CALL( CPXsetdblparam, env, CPX_PARAM_TILIM, limits.timeLimit );
	CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_NODELIM, limits.nodeLimit );
	CHECKED_CPX_CALL( CPXsetdblparam, env, CPX_PARAM_TRELIM, limits.memLimit );
	if (!limits.enableCuts) { CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_CUTPASS, -1 ); }
	if (!limits.enablePresolve) { CHECKED_CPX_CALL( CPXsetintparam, env, CPX_PARAM_PREIND, CPX_OFF ); }
	// launch B&B
	gLog().startSection("cpxBB");
	CHECKED_CPX_CALL( CPXsetinfocallbackfunc, env, infoCB, 0 );
	CHECKED_CPX_CALL( CPXmipopt, env, lp );
	CHECKED_CPX_CALL( CPXsetinfocallbackfunc, env, 0, 0 );
	int mipStat = CPXgetstat(env, lp);
	gLog().endSection();
	// get incumbent (if available)
	double primalBound = CPX_INFBOUND;
	if (CPXgetsolnpoolnumsolns(env, lp))
	{
		int n = CPXgetnumcols(env, lp);
		x.resize(n);
		CHECKED_CPX_CALL( CPXgetx, env, lp, &x[0], 0, n - 1 );
		CHECKED_CPX_CALL( CPXgetobjval, env, lp, &primalBound );
	}
	double dualBound;
	CHECKED_CPX_CALL( CPXgetbestobjval, env, lp, &dualBound );
	std::cout << "Primal bound: " << primalBound << std::endl;
	std::cout << "Dual bound: " << dualBound << std::endl;
	gLog().startSection("cpxBB");
	gLog().logItem("primalBound", primalBound);
	gLog().logItem("dualBound", dualBound);
	gLog().logItem("mipStat", mipStat);
	gLog().endSection();
	// revert paprameters
	CHECKED_CPX_CALL( CPXfreeusercuts, env, lp );
}

} // namespace dominiqs
