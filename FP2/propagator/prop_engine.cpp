/**
 * \file prop_engine.cpp
 *
 * Propagation Engine
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2008-2012
 */

#include <boost/bind.hpp>
#include <iostream>

#include <utils/floats.h>

#include "prop_engine.h"

using namespace dominiqs;

class PropagationEngineState : public State
{
public:
	PropagationEngineState(PropagationEngine* e) : engine(e), domainState(0), failed(false)
	{
		DOMINIQS_ASSERT( engine );
		DOMINIQS_ASSERT( engine->domain );
		domainState = engine->domain->getStateMgr();
		for (Propagator* p: engine->propagators)
		{
			StatePtr ps = p->getStateMgr();
			if (ps) propState.push_back(ps);
		}
	}
	virtual ~PropagationEngineState()
	{
		delete domainState;
		for (StatePtr ps: propState) delete ps;
		propState.clear();
	}
	void dump()
	{
		DOMINIQS_ASSERT( engine );
		DOMINIQS_ASSERT( domainState );
		domainState->dump();
		for (StatePtr ps: propState) ps->dump();
		failed = engine->hasFailed;
	}
	void restore()
	{
		DOMINIQS_ASSERT( domainState );
		domainState->restore();
		for (StatePtr ps: propState) ps->restore();
		engine->decisions.clear(); // need to thing about this!
		engine->hasFailed = failed; // need to thing about this!
	}
protected:
	PropagationEngine* engine;
	StatePtr domainState;
	std::vector<StatePtr> propState;
	bool failed;
};

PropagationEngine::~PropagationEngine()
{
	for (Propagator* p: propagators) delete p;
}

void PropagationEngine::setDomain(Domain* d)
{
	DOMINIQS_ASSERT( d );
	clear();
	domain = d;
	for (unsigned int j = 0; j < domain->size(); ++j)
	{
		vPropLbCount.push_back(0);
		vPropUbCount.push_back(0);
		advisors.push_back(std::list<AdvisorI*>());
	}

	domain->emitFixedBinUp = boost::bind(&PropagationEngine::fixedBinUp, this, _1);
	domain->emitFixedBinDown = boost::bind(&PropagationEngine::fixedBinDown, this, _1);
	domain->emitTightenedLb = boost::bind(&PropagationEngine::tightenedLb, this, _1, _2, _3);
	domain->emitTightenedUb = boost::bind(&PropagationEngine::tightenedUb, this, _1, _2, _3);
}

void PropagationEngine::pushPropagator(Propagator* prop)
{
	DOMINIQS_ASSERT( prop );
	DOMINIQS_ASSERT( prop->getDomain() == domain );
	propagators.push_back(prop);
	prop->setID(propagators.size() - 1);
	if (prop->pending()) queue[0].push_back(prop->getID());
	std::list<AdvisorI*> advs;
	prop->createAdvisors(advs);
	for (AdvisorI* adv: advs) advisors[adv->getVar()].push_back(adv);
}

bool PropagationEngine::propagate()
{
	lastFixed.clear();
	// propagation loop
	while(true)
	{
		Propagator* p = top();
		if (!p) break;
		if (p->pending()) p->propagate();
		if (p->failed()) hasFailed = true;
		if (stopPropagationIfFailed && hasFailed) break;
	}
	return (!hasFailed);
}

bool PropagationEngine::propagate(int var, double value)
{
	if (domain->isVarFixed(var)) return true;
	lastFixed.clear();
	if (domain->varType(var) == 'B')
	{
		if (isNull(value)) domain->fixBinDown(var);
		else domain->fixBinUp(var);
	}
	else
	{
		if (isNull(value - domain->varLb(var))) domain->tightenUb(var, value);
		else if (isNull(value - domain->varUb(var))) domain->tightenLb(var, value);
		else
		{
			domain->tightenLb(var, value);
			domain->tightenUb(var, value);
		}
	}
	decisions.push_back(Decision(var, value));
	// propagation loop
	while(true)
	{
		Propagator* p = top();
		if (!p) break;
		if (p->pending()) p->propagate();
		if (stopPropagationIfFailed && hasFailed) break;
	}
	return (!hasFailed);
}

bool PropagationEngine::propagate(const std::vector<int>& vars, const std::vector<double>& values)
{
	DOMINIQS_ASSERT( vars.size() == values.size() );
	unsigned int n = vars.size();
	int var;
	double value;
	lastFixed.clear();
	for (unsigned int i = 0; i < n; i++)
	{
		var = vars[i];
		value = values[i];
		if (domain->isVarFixed(var)) continue;
		if (domain->varType(var) == 'B')
		{
			if (isNull(value)) domain->fixBinDown(var);
			else domain->fixBinUp(var);
		}
		else
		{
			if (isNull(value - domain->varLb(var))) domain->tightenUb(var, value);
			else if (isNull(value - domain->varUb(var))) domain->tightenLb(var, value);
			else
			{
				domain->tightenLb(var, value);
				domain->tightenUb(var, value);
			}
		}
	}
	// propagation loop
	while(true)
	{
		Propagator* p = top();
		if (!p) break;
		if (p->pending()) p->propagate();
		if (stopPropagationIfFailed && hasFailed) break;
	}
	return (!hasFailed);
}

StatePtr PropagationEngine::getStateMgr()
{
	return new PropagationEngineState(this);
}

void PropagationEngine::clear()
{
	if (domain)
	{
		domain->emitFixedBinUp = 0;
		domain->emitFixedBinDown = 0;
		domain->emitTightenedLb = 0;
		domain->emitTightenedUb = 0;
		domain = 0;
	}

	vPropLbCount.clear();
	vPropUbCount.clear();
	for (std::list<AdvisorI*>& advs: advisors)
	{
		for (AdvisorI* adv: advs) delete adv;
	}
	advisors.clear();

	for (Propagator* p: propagators) delete p;
	propagators.clear();
	lastFixed.clear();
	decisions.clear();
}

void PropagationEngine::tightenedLb(int j, double newValue, double oldValue)
{
	double delta = newValue;
	bool wasUnbounded = true;
	if (greaterThan(oldValue, -INFBOUND))
	{
		delta -= oldValue;
		wasUnbounded = false;
	}
	if (domain->isVarFixed(j) && (domain->varType(j) != 'C')) lastFixed.push_back(j);
	bool propagateFlag = (domain->isVarFixed(j) || (vPropLbCount[j]++ < 10));
	for (AdvisorI* adv: advisors[j])
	{
		Propagator* p = adv->getPropagator();
		bool wasPending = p->pending();
		adv->tightenLb(delta, wasUnbounded, propagateFlag);
		if (p->pending() && !wasPending) queue[0].push_back(p->getID());
		if (p->failed()) hasFailed = true;
	}
}

void PropagationEngine::tightenedUb(int j, double newValue, double oldValue)
{
	double delta = newValue;
	bool wasUnbounded = true;
	if (lessThan(oldValue, INFBOUND))
	{
		delta -= oldValue;
		wasUnbounded = false;
	}
	if (domain->isVarFixed(j) && (domain->varType(j) != 'C')) lastFixed.push_back(j);
	bool propagateFlag = (domain->isVarFixed(j) || (vPropUbCount[j]++ < 10));
	for (AdvisorI* adv: advisors[j])
	{
		Propagator* p = adv->getPropagator();
		bool wasPending = p->pending();
		adv->tightenUb(delta, wasUnbounded, propagateFlag);
		if (p->pending() && !wasPending) queue[0].push_back(p->getID());
		if (p->failed()) hasFailed = true;
	}
}

void PropagationEngine::fixedBinUp(int j)
{
	lastFixed.push_back(j);
	for (AdvisorI* adv: advisors[j])
	{
		Propagator* p = adv->getPropagator();
		bool wasPending = p->pending();
		adv->fixedUp();
		if (p->pending() && !wasPending) queue[0].push_back(p->getID());
		if (p->failed()) hasFailed = true;
	}
}

void PropagationEngine::fixedBinDown(int j)
{
	lastFixed.push_back(j);
	for (AdvisorI* adv: advisors[j])
	{
		Propagator* p = adv->getPropagator();
		bool wasPending = p->pending();
		adv->fixedDown();
		if (p->pending() && !wasPending) queue[0].push_back(p->getID());
		if (p->failed()) hasFailed = true;
	}
}

Propagator* PropagationEngine::top()
{
	Queue::iterator itr = queue.begin();
	Queue::iterator end = queue.end();
	while (itr != end)
	{
		if (!(itr->second.empty()))
		{
			Propagator* p = propagators[itr->second.front()];
			itr->second.pop_front();
			return p;
		}
		++itr;
	}
	return 0;
}
