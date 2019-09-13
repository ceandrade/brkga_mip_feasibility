/**
 * @file domain.h
 * @brief Variables' domains Source
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2008
 */

#include "domain.h"

using namespace dominiqs;

void Domain::pushVar(const std::string& name, char t, double l, double u)
{
	names.push_back(name);
	type.push_back(t);
	lb.push_back(l);
	ub.push_back(u);
	fixed.push_back(equal(l, u));
}

StatePtr Domain::getStateMgr()
{
	return new DomainState(this);
}

void Domain::clear()
{
	names.clear();
	type.clear();
	lb.clear();
	ub.clear();
	fixed.clear();
}

DomainState* DomainState::clone() const
{
	return new DomainState(*this);
}

void DomainState::dump()
{
	lb = domain->lb;
	ub = domain->ub;
	fixed = domain->fixed;
}

void DomainState::restore()
{
	DOMINIQS_ASSERT( lb.size() == domain->names.size() );
	DOMINIQS_ASSERT( ub.size() == domain->names.size() );
	DOMINIQS_ASSERT( fixed.size() == domain->names.size() );
	DOMINIQS_ASSERT( domain->fixed.size() == domain->names.size() );
	domain->lb = lb;
	domain->ub = ub;
	domain->fixed = fixed;
}

