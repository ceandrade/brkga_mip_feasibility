/**
 * @file transformers.cpp
 * @brief Solution Transformers (a.k.a. rounders) Source
 *
 * @author Domenico Salvagnin <dominiqs at gmail dot com>
 * 2008
 */

#include <map>
#include <list>
#include <iterator>
#include <set>
#include <signal.h>
#include <cmath>
#include <numeric>
#include <iostream>
#include <algorithm>
#include <boost/format.hpp>

#include <utils/floats.h>
#include <utils/xmlconfig.h>
#include <utils/logger.h>

#include "transformers.h"

using namespace boost;
using namespace dominiqs;

// macro type savers

#define MYLOG(flag, actions) do { if (flag) { actions } } while(false);
#define LOG_START_SECTION(name) gLog().startSection(name)
#define LOG_END_SECTION() gLog().endSection()
#define LOG_ITEM(name, var) gLog().logItem(name, var)
#define READ_FROM_CONFIG( what, defValue ) what = gConfig().get("FeasibilityPump", #what, defValue)
#define LOG_CONFIG( what ) LOG_ITEM(#what, what)

// Rounders

static bool DEF_RANDOMIZED_ROUNDING = true;
static bool DEF_LOG_DETAILS = false;
static uint64_t DEF_SEED = 0;

SimpleRounding::SimpleRounding() : randomizedRounding(DEF_RANDOMIZED_ROUNDING), logDetails(DEF_LOG_DETAILS)
{
}

void SimpleRounding::readConfig()
{
//	READ_FROM_CONFIG( randomizedRounding, DEF_RANDOMIZED_ROUNDING );
	READ_FROM_CONFIG( randomizedRounding, false );
	READ_FROM_CONFIG( logDetails, DEF_LOG_DETAILS );
	LOG_START_SECTION("config");
	LOG_CONFIG( randomizedRounding );
	LOG_CONFIG( logDetails );
	LOG_END_SECTION();
	uint64_t seed = gConfig().get<uint64_t>("Globals", "seed", DEF_SEED);
	roundGen.setSeed(seed);
	roundGen.warmUp();
}

void SimpleRounding::init(const dominiqs::Model& model, bool ignoreGeneralInt)
{
	binaries.clear();
	gintegers.clear();
	integers.clear();
	for (unsigned int j = 0; j < model.numVars; j++)
	{
		if (different(model.xLb[j], model.xUb[j]))
		{
			if (model.xType[j] == 'B') binaries.push_back(j);
			if (model.xType[j] == 'I') gintegers.push_back(j);
		}
	}
	ignoreGeneralIntegers(ignoreGeneralInt);
	//MYLOG( logDetails, std::cout << "#integers: " << integers.size() << std::endl; );
}

void SimpleRounding::ignoreGeneralIntegers(bool flag)
{
	if (flag) integers = binaries;
	else
	{
		// append general integers to binaries
		integers.resize(binaries.size() + gintegers.size());
		std::vector<int>::iterator itr = copy(binaries.begin(), binaries.end(), integers.begin());
		std::vector<int>::iterator end = copy(gintegers.begin(), gintegers.end(), itr);
		DOMINIQS_ASSERT( end == integers.end() );
	}
}

void SimpleRounding::apply(const std::vector<double>& in, std::vector<double>& out)
{
	copy(in.begin(), in.end(), out.begin());
	int rDn = 0;
	int rUp = 0;
	double t = getRoundingThreshold(randomizedRounding, roundGen);
	for (int j: integers)
	{
		doRound(in[j], out[j], t);
		if (lessThan(out[j], in[j])) rDn++;
		if (greaterThan(out[j], in[j])) rUp++;
	}
	MYLOG( logDetails,
		gLog().logItem("roundDown", rDn);
		gLog().logItem("roundUp", rUp);
	);
}

PropagatorRounding::PropagatorRounding() : state(0)
{
}

void PropagatorRounding::readConfig()
{
	SimpleRounding::readConfig();
	std::string rankerName = gConfig().get("FeasibilityPump", "ranker", std::string("FRAC"));
	filterConstraints = gConfig().get("FeasibilityPump", "filterConstraints", true);
	LOG_START_SECTION("config");
	LOG_ITEM("ranker", rankerName);
	LOG_ITEM("filterConstraints", filterConstraints);
	LOG_END_SECTION();
	ranker = RankerPtr(RankerFactory::getInstance().create(rankerName));
	ranker->readConfig();
}

void PropagatorRounding::init(const dominiqs::Model& model, bool ignoreGeneralInt)
{
	SimpleRounding::init(model, ignoreGeneralInt);
	// add vars to domain
	for (unsigned int j = 0; j < model.numVars; j++) domain.pushVar(model.xNames[j], model.xType[j], model.xLb[j], model.xUb[j]);
	// connect domain to engine and ranker
	prop.setDomain(&domain);
	ranker->init(&domain, ignoreGeneralInt);
	// generate propagators with analyzers
	std::list<std::string> fNames;
	PropagatorFactories::getInstance().getIDs(std::back_insert_iterator< std::list<std::string> >(fNames));
	for (std::string name: fNames)
	{
		PropagatorFactoryPtr fact(PropagatorFactories::getInstance().create(name));
		factories[fact->getPriority()] = fact;
	}

	unsigned int filteredOut = 0;
	for (unsigned int i = 0; i < model.numRows; i++)
	{
		std::map<int, PropagatorFactoryPtr>::iterator itr = factories.begin();
		std::map<int, PropagatorFactoryPtr>::iterator end = factories.end();
		Constraint* c = model.rows[i].get();
		// constraint filter
		if (filterConstraints)
		{
			const int* idx = c->row.idx();
			const double* coef = c->row.coef();
			unsigned int size = c->row.size();
			bool allCont = true;
			double largest = std::numeric_limits<double>::min();
			double smallest = std::numeric_limits<double>::max();
			for (unsigned int k = 0; k < size; k++)
			{
				if (!domain.isVarFixed(idx[k]) && (domain.varType(idx[k]) != 'C'))
				{
					allCont = false;
					break;
				}
				double tmp = fabs(coef[k]);
				largest = std::max(largest, tmp);
				smallest = std::min(smallest, tmp);
			}
			double dynamism = (largest / smallest);
			if ((allCont && greaterThan(dynamism, 10.0)) || greaterThan(dynamism, 1000.0))
			{
				/*std::cout << c->name << ": ";
				for (unsigned int k = 0; k < size; k++)
				{
					std::cout << coef[k] << " ";
				}
				std::cout << c->sense << " " << c->rhs << std::endl;*/
				filteredOut++;
				continue;
			}
		}
		// try analyzers
		while (itr != end)
		{
			Propagator* p = itr->second->analyze(&domain, c);
			if (p)
			{
				prop.pushPropagator(p);
				break;
			}
			itr++;
		}
	}
	// log prop stats
	//std::cout << std::endl << "Propagators:" << std::endl;
	std::map<int, PropagatorFactoryPtr>::iterator itr = factories.begin();
	std::map<int, PropagatorFactoryPtr>::iterator end = factories.end();
	//while (itr != end)
	//{
		//std::cout << itr->second->getName() << " " << itr->second->created() << std::endl;
		//++itr;
	//}
	//std::cout << "Filtered out " << filteredOut << std::endl;

	// no initial propagation
	// prop.propagate();
	state = prop.getStateMgr();
	state->dump();
}

void PropagatorRounding::ignoreGeneralIntegers(bool flag)
{
	SimpleRounding::ignoreGeneralIntegers(flag);
	ranker->ignoreGeneralIntegers(flag);
}

void PropagatorRounding::apply(const std::vector<double>& in, std::vector<double>& out)
{
	copy(in.begin(), in.end(), out.begin());
	state->restore();
	double t = getRoundingThreshold(randomizedRounding, roundGen);
	ranker->setCurrentState(in);
	// main loop
	int next;
	while ((next = ranker->next()) >= 0)
	{
 		// standard rounding
		if (domain.varType(next) == 'B') doRound(in[next], out[next], t);
		else
		{
			// general integer variable: here we take into account also the tightened domain
			// in order to have a more clever rounding!
			if (lessEqualThan(in[next], domain.varLb(next))) out[next] = domain.varLb(next);
			else if (greaterEqualThan(in[next], domain.varUb(next))) out[next] = domain.varUb(next);
			else doRound(in[next], out[next], t);
		}
		// propagate
		prop.propagate(next, out[next]);
		DOMINIQS_ASSERT( domain.isVarFixed(next) );
		// update with fixings
		for (int j: prop.getLastFixed()) out[j] = domain.varLb(j);
	}
}

void PropagatorRounding::clear()
{
	// log prop stats
	//std::cout << std::endl << "Propagators statistics (#prop called, #domain red):" << std::endl;
	std::map<int, PropagatorFactoryPtr>::iterator itr = factories.begin();
	std::map<int, PropagatorFactoryPtr>::iterator end = factories.end();
	while (itr != end)
	{
		//std::cout << itr->second->getName() << " " << itr->second->propCalled() << " " << itr->second->domainReductions() << std::endl;
		++itr;
	}
	// clear
	delete state;
	prop.clear();
	factories.clear();
}

// auto registration
// please register your class here with an appropriate name

class TR_FACTORY_RECORDER
{
public:
	TR_FACTORY_RECORDER()
	{
		//std::cout << "Registering SolutionTransformers...";
		TransformersFactory::getInstance().registerClass<SimpleRounding>("std");
		TransformersFactory::getInstance().registerClass<PropagatorRounding>("propround");
		//std::cout << "done" << std::endl;
	}
};

TR_FACTORY_RECORDER my_tr_factory_recorder;
