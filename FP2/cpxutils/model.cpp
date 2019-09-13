/**
* @file model.cpp
* @brief Model Class
*
* @author Domenico Salvagnin dominiqs@gmail.com
* Copyright 2009 Domenico Salvagnin
*/

#include "model.h"

namespace dominiqs {

Model::Model(const Model& other)
{
	bigRange = other.bigRange;
	linearEPS = other.linearEPS;
	ageLimit = other.ageLimit;
	env = other.env;
	if (env && other.lp) { CLONE_PROB(env, other.lp, lp); }
	else lp = 0;
	numVars = other.numVars;
	numBoundedVars = other.numBoundedVars;
	numRows = other.numRows;
	numOrigRows = other.numOrigRows;
	numSlacks = other.numSlacks;
	xLb = other.xLb;
	xUb = other.xUb;
	xType = other.xType;
	xSimplexType = other.xSimplexType;
	xNames = other.xNames;
	xObj = other.xObj;
	rows = other.rows;
	sense = other.sense;
	rhs = other.rhs;
	cols = other.cols;
}

void Model::extract(dominiqs::Env _env, dominiqs::Prob _lp, const std::vector<char>& cType)
{
	DOMINIQS_ASSERT( _env && _lp );
	env = _env;
	lp = _lp;
	numVars = (unsigned int)CPXgetnumcols(env, lp);
	numRows = (unsigned int)CPXgetnumrows(env, lp);
	numOrigRows = numRows;

	// read structural var types
	if (cType.size())
	{
		DOMINIQS_ASSERT( cType.size() == numVars );
		xType = cType;
	}
	else
	{
		xType.resize(numVars);
		if (CPXgetprobtype(env, lp) != CPXPROB_LP) { CHECKED_CPX_CALL( CPXgetctype, env, lp, &xType[0], 0, numVars - 1 ); }
		else std::fill(xType.begin(), xType.end(), 'C');
	}

	// get var data
	xLb.resize(numVars);
	xUb.resize(numVars);
	xObj.resize(numVars);
	xSimplexType.resize(numVars);
	CHECKED_CPX_CALL( CPXgetlb, env, lp, &xLb[0], 0, numVars - 1 );
	CHECKED_CPX_CALL( CPXgetub, env, lp, &xUb[0], 0, numVars - 1 );
	CHECKED_CPX_CALL( CPXgetobj, env, lp, &xObj[0], 0, numVars - 1 );

	// get row data
	sense.resize(numRows);
	rhs.resize(numRows);
	CHECKED_CPX_CALL( CPXgetsense, env, lp, &sense[0], 0, numRows - 1 );
	CHECKED_CPX_CALL( CPXgetrhs, env, lp, &rhs[0], 0, numRows - 1 );

	// get matrix row-wise
	int nnz = CPXgetnumnz(env, lp);
	dominiqs::numarray<int> matbeg(numRows + 1);
	dominiqs::numarray<int> matind(nnz);
	dominiqs::numarray<double> matval(nnz);
	int requiredSpace = 0;
	int nElems = 0;
	CHECKED_CPX_CALL( CPXgetrows, env, lp, &nElems, matbeg.c_ptr(), matind.c_ptr(), matval.c_ptr(), nnz, &requiredSpace, 0, numRows - 1 );
	DOMINIQS_ASSERT( requiredSpace >= 0 );
	DOMINIQS_ASSERT( nElems == nnz );
	matbeg[numRows] = nnz;

	// read rows
	std::vector< std::string > rNames;
	getConstrNames(env, lp, rNames);
	numSlacks = 0;
	rows.resize(numRows);
	for (unsigned int i = 0; i < numRows; i++)
	{
		CutPtr r(new Cut);
		r->name = rNames[i];
		r->row.copy(matind.c_ptr() + matbeg[i], matval.c_ptr() + matbeg[i], matbeg[i+1] - matbeg[i]);
		r->sense = sense[i];
		r->rhs = rhs[i];
		rows[i] = r;
		if (r->sense == 'R') throw std::runtime_error("We do not support ranged constraints!");
		if (r->sense != 'E') numSlacks++;
	}

	// get matrix col-wise
	matbeg.resize(numVars + 1);
	CHECKED_CPX_CALL( CPXgetcols, env, lp, &nElems, matbeg.c_ptr(), matind.c_ptr(), matval.c_ptr(), nnz, &requiredSpace, 0, numVars - 1 );
	DOMINIQS_ASSERT( requiredSpace >= 0 );
	DOMINIQS_ASSERT( nElems == nnz );
	matbeg[numVars] = nnz;

	// read cols
	getVarNames(env, lp, xNames);
	numBoundedVars = 0;
	cols.resize(numVars);
	for (unsigned int j = 0; j < numVars; j++)
	{
		cols[j] = SparseVectorPtr(new SparseVector);
		cols[j]->copy(matind.c_ptr() + matbeg[j], matval.c_ptr() + matbeg[j], matbeg[j+1] - matbeg[j]);
		if (isNull(xLb[j], linearEPS)) xLb[j] = 0.0;
		if (isNull(xUb[j], linearEPS)) xUb[j] = 0.0;
		if (xLb[j] <= -CPX_INFBOUND)
		{
			if (xUb[j] >= CPX_INFBOUND) xSimplexType[j] = 'R';
			else xSimplexType[j] = 'N';
		}
		else
		{
			if (xUb[j] >= CPX_INFBOUND) xSimplexType[j] = 'P';
			else
			{
				xSimplexType[j] = 'B';
				++numBoundedVars;
			}
		}
		if (equal(xLb[j], xUb[j], linearEPS)) xSimplexType[j] = 'F';
		if (xSimplexType[j] == 'B' && ((xUb[j] - xLb[j]) > bigRange)) xSimplexType[j] = 'H';
	}

	// calculate slack types
	for (CutPtr r: rows) calculateSlackType(r);

	computeLocks();
}

void Model::switchToLP()
{
	DOMINIQS_ASSERT( env && lp );
	CHECKED_CPX_CALL( CPXchgprobtype, env, lp, CPXPROB_LP );
}

void Model::switchToMIP()
{
	DOMINIQS_ASSERT( env && lp );
	CHECKED_CPX_CALL( CPXchgprobtype, env, lp, CPXPROB_MILP );
	CHECKED_CPX_CALL( CPXcopyctype, env, lp, &xType[0] );
}

void Model::optimize()
{
	DOMINIQS_ASSERT( env && lp );
	int probType = CPXgetprobtype(env, lp);
	if (probType == CPXPROB_LP) CHECKED_CPX_CALL( CPXlpopt, env, lp );
	else CHECKED_CPX_CALL( CPXmipopt, env, lp );
}

double Model::getObjVal() const
{
	double objval;
	CHECKED_CPX_CALL( CPXgetobjval, env, lp, &objval );
	return objval;
}

void Model::addVar(const std::string& name, double lb, double ub, char type, double obj)
{
	++numVars;
	xNames.push_back(name);
	if (isNull(lb, linearEPS)) lb = 0.0;
	if (isNull(ub, linearEPS)) ub = 0.0;
	char simplexType;
	if (lb <= -CPX_INFBOUND)
	{
		if (ub >= CPX_INFBOUND) simplexType = 'R';
		else simplexType = 'N';
	}
	else
	{
		if (ub >= CPX_INFBOUND) simplexType = 'P';
		else
		{
			simplexType = 'B';
			++numBoundedVars;
		}
	}
	if (equal(lb, ub, linearEPS)) simplexType = 'F';
	if ((simplexType == 'B') && ((ub - lb) > bigRange)) simplexType = 'H';
	xSimplexType.push_back(simplexType);
	xLb.push_back(lb);
	xUb.push_back(ub);
	xType.push_back(type);
	xObj.push_back(obj);
	if (env && lp) dominiqs::addEmptyVar(env, lp, name, type, lb, ub, obj);
}

void Model::addCut(CutPtr row)
{
	if (!row) return;
	DOMINIQS_ASSERT( env && lp );
	row->inUse = true;
	rows.push_back(row);
	sense.push_back(row->sense);
	rhs.push_back(row->rhs);
	calculateSlackType(row);
	dominiqs::addCut(env, lp, row->name, row->row, row->sense, row->rhs);
	numRows++;
	if (row->sense != 'E') numSlacks++;
}

int Model::purgeCuts(const std::vector<double>& x)
{
	DOMINIQS_ASSERT( env && lp );
	int deleted = 0;
	std::vector<int> delStat(numRows);
	// update age and decide which are the cuts to remove
	for (unsigned int i = numOrigRows; i < numRows; i++)
	{
		if (rows[i]->isSlack(&x[0], linearEPS) && rows[i]->removable) rows[i]->age++;
		else rows[i]->age = 0;
		if (rows[i]->age > ageLimit)
		{
			delStat[i] = 1;
			deleted++;
		}
	}
	// remove cuts from Cplex
	CHECKED_CPX_CALL( CPXdelsetrows, env, lp, &delStat[0] );
	DOMINIQS_ASSERT( ((int)numRows - deleted) == CPXgetnumrows(env, lp) );
	// remove cuts from Model too
	for (unsigned int i = numOrigRows; i < numRows; i++)
	{
		if (delStat[i] == -1) rows[i]->inUse = false;
		else rows[delStat[i]] = rows[i];
	}
	numRows = (unsigned int)CPXgetnumrows(env, lp);
	rows.resize(numRows);
	sense.resize(numRows);
	rhs.resize(numRows);
	numSlacks = 0;
	if (numRows)
	{
		CHECKED_CPX_CALL( CPXgetsense, env, lp, &sense[0], 0, numRows - 1 );
		CHECKED_CPX_CALL( CPXgetrhs, env, lp, &rhs[0], 0, numRows - 1 );
		for (unsigned int i = 0; i < numRows; i++) if (rows[i]->sense != 'E') numSlacks++;
	}
	return deleted;
}

int Model::purgeSlackCuts(const std::vector<double>& x)
{
	DOMINIQS_ASSERT( env && lp );
	int deleted = 0;
	std::vector<int> delStat(numRows);
	for (unsigned int i = numOrigRows; i < numRows; i++)
	{
		if (rows[i]->isSlack(&x[0], linearEPS) && rows[i]->removable)
		{
			delStat[i] = 1;
			deleted++;
		}
	}

	// remove cuts from Cplex
	CHECKED_CPX_CALL( CPXdelsetrows, env, lp, &delStat[0] );
	DOMINIQS_ASSERT( ((int)numRows - deleted) == CPXgetnumrows(env, lp) );
	// remove cuts from Model too
	for (unsigned int i = numOrigRows; i < numRows; i++)
	{
		if (delStat[i] == -1) rows[i]->inUse = false;
		else rows[delStat[i]] = rows[i];
	}
	numRows = (unsigned int)CPXgetnumrows(env, lp);
	rows.resize(numRows);
	sense.resize(numRows);
	rhs.resize(numRows);
	numSlacks = 0;
	if (numRows)
	{
		CHECKED_CPX_CALL( CPXgetsense, env, lp, &sense[0], 0, numRows - 1 );
		CHECKED_CPX_CALL( CPXgetrhs, env, lp, &rhs[0], 0, numRows - 1 );
		for (unsigned int i = 0; i < numRows; i++) if (rows[i]->sense != 'E') numSlacks++;
	}
	return deleted;
}

void Model::purgeAll()
{
	DOMINIQS_ASSERT( env && lp );
	if (numOrigRows == numRows) return;
	CHECKED_CPX_CALL( CPXdelrows, env, lp, numOrigRows, numRows - 1 );
	DOMINIQS_ASSERT( ((int)numOrigRows) == CPXgetnumrows(env, lp) );
	// remove cuts from Model too
	for (unsigned int i = numOrigRows; i < numRows; i++) rows[i]->inUse = false;
	numRows = (unsigned int)CPXgetnumrows(env, lp);
	rows.resize(numRows);
	sense.resize(numRows);
	rhs.resize(numRows);
	numSlacks = 0;
	if (numRows)
	{
		CHECKED_CPX_CALL( CPXgetsense, env, lp, &sense[0], 0, numRows - 1 );
		CHECKED_CPX_CALL( CPXgetrhs, env, lp, &rhs[0], 0, numRows - 1 );
		for (unsigned int i = 0; i < numRows; i++) if (rows[i]->sense != 'E') numSlacks++;
	}
}

bool Model::isLinearFeasible(const std::vector<double>& x) const
{
	DOMINIQS_ASSERT( x.size() == numVars );

	// check bounds
	for (unsigned int i = 0; i < numVars; i++)
	{
		if (lessThan(x[i], xLb[i], linearEPS) || greaterThan(x[i], xUb[i], linearEPS)) return false;
	}

	// check constraints
	for (unsigned int i = 0; i < numOrigRows; i++)
	{
		if (!rows[i]->satisfiedBy(&x[0], linearEPS)) return false;
	}
	return true;
}

bool Model::isIntegerFeasible(const std::vector<double>& x) const
{
	DOMINIQS_ASSERT( x.size() == numVars );
	for (unsigned int i = 0; i < numVars; i++)
	{
		if ((xType[i] != 'C') && (!isInteger(x[i], linearEPS))) return false;
	}
	return true;
}

bool Model::isFeasible(const std::vector<double>& x) const
{
	return (isIntegerFeasible(x) && isLinearFeasible(x));
}

int Model::reducedCostFixing(double primalBound)
{
	DOMINIQS_ASSERT( env && lp );
	double dualBound = getObjVal();
	dominiqs::numarray<double> red(numVars);
	CHECKED_CPX_CALL( CPXgetdj, env, lp, &red[0], 0, numVars - 1 );
	dominiqs::numarray<int> cstat(numVars);
	dominiqs::numarray<int> rstat(numRows);
	CHECKED_CPX_CALL( CPXgetbase, env, lp, &cstat[0], &rstat[0] );

	int fixed = 0;
	char change = 'B';
	double delta = fabs(primalBound - dualBound);
	for (unsigned int j = 0; j < numVars; j++)
	{
		if ((cstat[j] != CPX_BASIC) && different(xLb[j], xUb[j]) && greaterThan(fabs(red[j]), delta))
		{
			int idx = (int)j;
			double val = (cstat[j] == CPX_AT_LOWER) ? xLb[j] : xUb[j];
			CHECKED_CPX_CALL( CPXchgbds, env, lp, 1, &idx, &change, &val );
			xLb[j] = xUb[j] = val;
			fixed++;
		}
	}
	return fixed;
}

void Model::calculateSlackType(CutPtr r)
{
	if (r->slackType != 'U') return;
	if (r->sense == 'E' || (!isInteger(r->rhs, linearEPS))) r->slackType = 'C';
	else
	{
		const int* idx = r->row.idx();
		const double* coef = r->row.coef();
		for (unsigned int k = 0; k < r->row.size(); k++)
		{
			if (xType[idx[k]] == 'C' || (!isInteger(coef[k], linearEPS)))
			{
				r->slackType = 'C';
				return;
			}
		}
		r->slackType = 'I';
	}
}

void Model::computeLocks()
{
	upLocks.resize(numVars);
	std::fill(upLocks.begin(), upLocks.end(), 0);
	downLocks.resize(numVars);
	std::fill(downLocks.begin(), downLocks.end(), 0);
	
	for (CutPtr row: rows)
	{
		unsigned int size = row->row.size();
		const int* idx = row->row.idx();
		const double* coef = row->row.coef();
		if (row->sense == 'E')
		{
			for (unsigned int k = 0; k < size; k++)
			{
				upLocks[idx[k]]++;
				downLocks[idx[k]]++;
			}
		}
		else if (row->sense == 'L')
		{
			for (unsigned int k = 0; k < size; k++)
			{
				if (isPositive(coef[k])) upLocks[idx[k]]++;
				else downLocks[idx[k]]++;
			}
		}
		else //< (row->sense == 'G')
		{
			for (unsigned int k = 0; k < size; k++)
			{
				if (isPositive(coef[k])) downLocks[idx[k]]++;
				else upLocks[idx[k]]++;
			}
		}
	}
}

} // namespace dominiqs
