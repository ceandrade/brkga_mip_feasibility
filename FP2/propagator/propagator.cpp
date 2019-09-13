/**
 * \file propagator.cpp
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2008-2012
 */

#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <iostream>

#include <utils/floats.h>

#include "propagator.h"

using namespace dominiqs;

const char* PropagatorStateName[] = {
	"unknown",
	"entailed",
	"strong entailed",
	"infeasible"
};

PropagatorFactory::PropagatorFactory() : numCreated(0), numPropCalled(0), numDomainReductions(0) {}

void PropagatorFactory::reset()
{
	numCreated = 0;
	numPropCalled = 0;
	numDomainReductions = 0;
}
