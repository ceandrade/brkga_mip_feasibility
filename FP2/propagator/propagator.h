/**
 * @file propagator.h
 * @brief Constraint Propagator Interface
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2008-2012
 */

#ifndef PROPAGATOR_H
#define PROPAGATOR_H

#include <vector>
#include <list>
#include <map>
#include <deque>
#include <string>
#include <iosfwd>

#include <utils/singleton.h>
#include <utils/factory.h>
#include <utils/maths.h>
#include <utils/asserter.h>

#include "domain.h"
#include "history.h"

/**
 * @brief Propagator State
 */

enum PropagatorState {
	CSTATE_UNKNOWN = 0,
	CSTATE_ENTAILED = 1,
	CSTATE_STRONG_ENTAILED = 2,
	CSTATE_INFEAS = 3
};

extern const char* PropagatorStateName[];

class PropagatorFactory;
class AdvisorI;
class Domain;

/**
 * @brief Propagator interface
 *
 * A propagator implements domain filtering for a constraint
 */

class Propagator
{
public:
	Propagator(Domain* d, PropagatorFactory* fact = 0)
		: domain(d), factory(fact), id(-1), priority(0), dirty(true), state(CSTATE_UNKNOWN) {}
	virtual ~Propagator() {}
	/**
	 * Create the advisors needed to effectively propagate and return them
	 */
	virtual void createAdvisors(std::list<AdvisorI*>& advisors) {}
	//@{
	/**
	 * get/set propagator attributes
	 */
	inline void setName(const std::string& n) { name = n; }
	inline const std::string& getName() const { return name; }
	inline int getID() const { return id; }
	inline void setID(int _id) { id = _id; }
	inline int getPriority() const { return priority; }
	inline void setPriority(int p) { priority = p; }
	inline Domain* getDomain() const { return domain; }
	inline void setPending() { dirty = true; }
	inline bool pending() const { return dirty; }
	inline PropagatorState getState() const { return state; }
	inline bool failed() const { return (state == CSTATE_INFEAS); }
	//@}
	/**
	 * Do a one-pass propagation of the constraint
	 */
	virtual void propagate() =0;
	// get a state handler
	virtual StatePtr getStateMgr() { return 0; }
	// output (virtual output idiom)
	virtual std::ostream& print(std::ostream& out) const { return out; }
protected:
	// data
	Domain* domain;
	PropagatorFactory* factory;
	int id;
	int priority;
	bool dirty;
	PropagatorState state;
	std::string name;
	// helpers
	virtual void updateState() {}
};

inline std::ostream& operator<<(std::ostream& out, const Propagator& prop) { return prop.print(out); }

/**
 * PropagatorFactory interface
 * This class analyzes a constraint and, if it detects a given structure,
 * instantiate a specialized constraint propagator
 */

class PropagatorFactory
{
public:
	PropagatorFactory();
	virtual ~PropagatorFactory() {}
	virtual PropagatorFactory* clone() const =0;
	virtual int getPriority() const =0;
	virtual const char* getName() const =0;
	virtual Propagator* analyze(Domain* d, dominiqs::Constraint* c) =0;
	// stats
	void reset();
	inline int created() const { return numCreated; }
	inline int& propCalled() { return numPropCalled; }
	inline int& domainReductions() { return numDomainReductions; }
protected:
	// stats
	int numCreated;
	int numPropCalled;
	int numDomainReductions;
};

typedef boost::shared_ptr<PropagatorFactory> PropagatorFactoryPtr;

typedef dominiqs::SingletonHolder< dominiqs::Factory<PropagatorFactory, std::string> > PropagatorFactories;

#endif /* PROPAGATOR_H */
