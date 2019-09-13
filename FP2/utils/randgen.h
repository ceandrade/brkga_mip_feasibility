/**
 * @file randgen.h
 * @brief Random Number Generator
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2007
 */

#ifndef RANDGEN_H
#define RANDGEN_H

#include <cmath>
#include <ctime>
#include <boost/random/linear_congruential.hpp>

#include "asserter.h"

namespace dominiqs {

/**
 * Generate a seed for the generator based on user input
 * If user input is not null, use it as seed
 * Otherwise use current timestamp
 */

inline uint64_t generateSeed(uint64_t seed)
{
	if (seed) return seed;
	return (uint64_t)(std::time(0));
}

/**
 * Basic Random numbers generator class
 * Generates a floating number in the range [0,1) with uniform distribution
 */

class UnitRandGen
{
public:
	/**
	 * default constructor
	 * @param _seed (uses generateSeed!)
	 */
	UnitRandGen(uint64_t _seed = 0) : rnd(generateSeed(_seed)), span(1.0)
	{
		span = double(rnd.max()) - double(rnd.min());
	}
	/**
	 * Seed the generator
	 * @param _seed: if 0 use the current time, otherwise the supplied seed
 	 */
	void setSeed(uint64_t _seed) { rnd.seed(generateSeed(_seed)); }
	/** @return a random floating point value in the specified range */
	double getFloat() { return (rnd() / span); }
	/** call the generator a few times to discard first values */
	void warmUp() { for (int i = 0; i < WARMUP_TRIES; i++) rnd(); }
protected:
	boost::rand48 rnd;
	double span;
	static const int WARMUP_TRIES = 1000;
};

/**
 * Random numbers generator class
 * Generates numbers in a range in a uniform distribution
 */

class RandGen : public UnitRandGen
{
public:
	/**
	 * default constructor
	 * @param _min lower range value
	 * @param _max upper range value
	 * @param _seed (uses generateSeed!)
	 */
	RandGen(double _min = -1, double _max = 1, uint64_t _seed = 0) : UnitRandGen(_seed), min(_min), max(_max) {}
	/**
	 * set uniform distribution range
	 * @param _min lower range value
	 * @param _max upper range value
	 */
	void setRange(double _min, double _max) { min = _min; max = _max; }
	/** @return a random floating point value in the specified range */
	double getFloat() { return ((rnd() / span) * (max - min) + min); }
	/** @return a random integer value in the specified range */
	long getInteger() { return lrint(getFloat()); }
private:
	double min;
	double max;
};

/**
 * Random numbers generator class
 * Generates integers in the range [0,N) in a uniform distribution
 * This interface is needed for STL algorithms
 */

class STLRandGen : public UnitRandGen
{
public:
	/**
	 * default constructor
	 * @param _seed (uses generateSeed!)
	 */
	STLRandGen(uint64_t _seed = 0) : UnitRandGen(_seed) {}
	long operator()(long N)
	{
		long res = lrint(getFloat() * (N - 1));
		DOMINIQS_ASSERT( res >= 0 );
		DOMINIQS_ASSERT( res < N );
		return res;
	}
};

/**
 * Random alphanumeric character generator
 */
namespace details
{
	static const char* genChars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
}

class RandCharGen
{
public:
	/**
	 * default constructor
	 * @param _seed (uses generateSeed!)
	 */
	RandCharGen(uint64_t _seed = 0) : rnd(generateSeed(_seed))
	{
		span = double(rnd.max()) - double(rnd.min());
	}
	/**
	 * Seed the generator
	 * @param _seed: if 0 use the current time, otherwise the supplied seed
 	 */
	void setSeed(uint64_t _seed) { rnd.seed(_seed); }
	/** @return random character */
	char operator()()
	{
		int idx = int(double(rnd()) / span * RANDGEN_NUM_CHARS);
		DOMINIQS_ASSERT( idx >= 0 );
		return details::genChars[idx];
	}
	/** call the generator a few times to discard first values */
	void warmUp() { for (int i = 0; i < WARMUP_TRIES; i++) rnd(); }
private:
	boost::rand48 rnd;
	double span;
	static const int RANDGEN_NUM_CHARS = 62;
	static const int WARMUP_TRIES = 1000;
};

} // namespace dominiqs

#endif /* RANDGEN_H */
