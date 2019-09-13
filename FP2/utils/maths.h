/**
 * @file maths.h
 * @brief Mathematical utilities Header
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2008
 */

#ifndef MATHS_H
#define MATHS_H

#include <cmath>
#include <vector>
#include <string>
#include <functional>
#include <boost/shared_ptr.hpp>
#include <boost/intrusive_ptr.hpp>

#include "asserter.h"
#include "floats.h"
#include "machine_utils.h"
#include "managed_object.h"

namespace dominiqs {

/**
 * Sparse Vector
 * Stores a vector in sparse form
 * The internal vectors have 16-bit alignment (SSE2 compatible)
 */

class SparseVector
{
public:
	typedef unsigned int size_type;
	// constructors
	SparseVector() : m_idx(0), m_coef(0), length(0), alloc(0) {}
	SparseVector(const SparseVector& other);
	// assignment operator
	SparseVector& operator=(const SparseVector& other);
	// destructor
	inline ~SparseVector() throw()
	{
		freeSSE2(m_idx);
		freeSSE2(m_coef);
	}
	inline void swap(SparseVector& other) throw()
	{
		std::swap(m_idx, other.m_idx);
		std::swap(m_coef, other.m_coef);
		std::swap(length, other.length);
		std::swap(alloc, other.alloc);
	}
	// size
	inline size_type size() const { return length; }
	inline size_type capacity() const { return alloc; }
	void resize(size_type newSize);
	inline bool empty() const { return (length == 0); }
	void reserve(size_type n);
	inline void clear() { length = 0; }
	// push/pop
	inline void push(int i, double v)
	{
		if (length == alloc) reserve(std::max(2 * alloc, (size_type)8));
		push_unsafe(i, v);
	}
	inline void push_unsafe(int i, double v)
	{
		m_idx[length] = i;
		m_coef[length] = v;
		++length;
	}
	inline void pop()
	{
		if (length) --length;
	}
	void copy(const int* idx, const double* coef, size_type cnt);
	// get data
	//@{
	int* idx() { return m_idx; }
	const int* idx() const { return m_idx; }
	double* coef() { return m_coef; }
	const double* coef() const { return m_coef; }
	//@}
	/**
	* Compare two sparse vectors for equality
	* ATTENTION: the procedure assumes the indexes to be in increasing order, i.e., sorted!!!
	*/
	bool operator==(const SparseVector& rhs) const;
	/** conversions to/from dense vectors */
	/** Shrink a dense vector into a sparse one */
	void gather(const double* in, int n, double eps = defaultEPS);
	/** Expand a sparse vector into a dense one */
	void scatter(double* out, int n, bool reset = false);
	/** Zero out the coefficients of a dense vector corresponding to the support of a sparse vector */
	void unscatter(double* out);
protected:
	int* m_idx; //< indexes
	double* m_coef; //< coefficients
	size_type length;
	size_type alloc;
};

typedef boost::shared_ptr<SparseVector> SparseVectorPtr;

typedef boost::shared_ptr< std::vector<int> > IntegerVectorPtr;

typedef boost::shared_ptr< std::vector<double> > FloatVectorPtr;

/**
 * Linear Constraint
 */

class Constraint : private ManagedObject<Constraint>
{
public:
	/** @name Data */
	//@{
	std::string name; //< constraint name
	SparseVector row; /**< coefficient row \f$ a_i \f$ */
	double rhs; /**< right hand side \f$ b_i \f$ */
	char sense; /**< constraint sense */
	//@}
	Constraint* clone() const { return new Constraint(*this); }
	/**
	 * Check if this constraint is satisfied by assignment x, with tolerance eps
	 */
	bool satisfiedBy(const double* x, double eps = defaultEPS) const;
	/**
	 * Compute the violation of constraint by assignment x
	 * A positive value means a constraint violation,
	 * while a negative one means constraint is slack
	 */
	double violation(const double* x) const;
	/**
	 * Check if a constraint is slack w.r.t. assignment x, with tolerance eps
	 * @return true if constraint is slack, false otherwise
	 */
	bool isSlack(const double* x, double eps = defaultEPS) const;
};

inline bool Constraint::satisfiedBy(const double* x, double eps) const
{
	return (!isPositive(violation(x), eps));
}

inline bool Constraint::isSlack(const double* x, double eps) const
{
	return isNegative(violation(x), eps);
}

typedef boost::intrusive_ptr<Constraint> ConstraintPtr;

/**
 * Initialize the range [first,last) of elements with consecutive elements, starting from @param value
 * iota is not part of the C++ standard (it is an GNU extension), hence this implementation
 * @tparam ForwardIterator iterator type
 * @tparam T value type
 * @param first first element of the range
 * @param last one past last element of the range
 * @param value starting value
 */

template<typename ForwardIterator, typename T>
void iota(ForwardIterator first, ForwardIterator last, T value)
{
	while (first != last) *first++ = value++;
}

/**
 * Performe the operation: v <- v + lambda w
 * where both v and w are dense vectors and lambda is a scalar
 */

void accumulate(double* v, const double* w, int n, double lambda = 1.0);

/**
 * Performe the operation: v <- v + lambda w
 * where v is a dense vector, w is sparse and lambda is a scalar
 */

void accumulate(double* v, const int* wIdx, const double* wCoef, int n, double lambda = 1.0);

inline void accumulate(double* v, const SparseVector& w, double lambda = 1.0)
{
	accumulate(v, w.idx(), w.coef(), w.size(), lambda);
}

/**
 * Scale a dense vector v multiplying it by lambda: v <- lambda v
 */

void scale(double* v, int n, double lambda = 1.0);

/**
 * Dot Product between dense vectors (manual loop unrolling)
 */

double dotProduct(const double* a1, const double* a2, int n);

/**
 * Dot Product between a sparse and a dense vector
 */

double dotProduct(const int* idx1, const double* a1, int n, const double* a2);

inline double dotProduct(const SparseVector& a1, const double* a2)
{
	return dotProduct(a1.idx(), a1.coef(), a1.size(), a2);
}

/**
 * Checks if two dense vectors have disjoint support
 */

bool disjoint(const double* a1, const double* a2, int n);

/**
 * Euclidian norm on a dense vector
 */

double euclidianNorm(const double* v, int n);

/**
 * Euclidian distance between two dense vectors
 */

double euclidianDistance(const double* a1, const double* a2, int n);

/**
 * Lexicographically compare two array of doubles
 * @param s1 first array
 * @param s2 second array
 * @param n arrays size
 * @return comparison result {-1, 0, 1} if {<, =, >} respectively
 */

int lexComp(const double* s1, const double* s2, int n);

/**
 * Unary predicate that incrementally compute the variance of a list of numbers
 * The results can be obtained with result()
 * The flag fromSample decides if the variance was from the entire population
 * or a subset of observation, i.e. if fromSample is true the sum of squares is
 * divided by n - 1, otherwise by n
 */

class IncrementalVariance : public std::unary_function<double, void>
{
public:
	IncrementalVariance() : cnt(0), mean(0.0), sumsq(0.0) {}
	void operator() (double x)
	{
		cnt++;
		double delta = x - mean;
		mean += delta / cnt;
		sumsq += delta * (x - mean);
	}
	int count() const { return cnt; }
	double result(bool fromSample = false) const { return (cnt > 1) ? (sumsq / (cnt - int(fromSample))) : 0.0; }
protected:
	int cnt;
	double mean;
	double sumsq;
};

/**
 * Return the variance of a list of numbers
 * The flag fromSample decides if the variance was from the entire population
 * or a subset of observation, i.e. if fromSample is true the sum of squares is
 * divided by n - 1, otherwise by n
 */

template<typename ForwardIterator>
double variance(ForwardIterator first, ForwardIterator last, bool fromSample = false)
{
	int cnt = 0;
	double sum = 0.0;
	ForwardIterator firstCopy = first;
	while (firstCopy != last)
	{
		cnt++;
		sum += *firstCopy++;
	}
	double mean = sum / cnt;
	sum = 0.0;
	while (first != last)
	{
		sum += std::pow(*first++ - mean, 2.0);
	}
	return (cnt > 1) ? (sum / (cnt - int(fromSample))) : 0.0;
}

/**
 * Unary predicate that incrementally compute the geometric mean of a list of numbers
 * Each number must be positive!
 */

class IncrementalGeomMean : public std::unary_function<double, void>
{
public:
	IncrementalGeomMean() : cnt(0), sum(0.0) {}
	void operator() (double x)
	{
		DOMINIQS_ASSERT( x > 0.0 );
		cnt++;
		sum += std::log(x);
	}
	int count() const { return cnt; }
	double result() const { return (cnt > 0) ? std::exp(sum / cnt) : 0.0; }
protected:
	int cnt;
	double sum;
};

/**
 * Return the geometric mean of a list of postive numbers
 */

template<typename ForwardIterator>
double geomMean(ForwardIterator first, ForwardIterator last)
{
	return std::for_each(first, last, IncrementalGeomMean()).result();
}

/**
 * Unary predicate that incrementally compute the arithmetic mean of a list of numbers
 */

class IncrementalMean : public std::unary_function<double, void>
{
public:
	IncrementalMean() : cnt(0), sum(0.0) {}
	void operator() (double x)
	{
		cnt++;
		sum += x;
	}
	int count() const { return cnt; }
	double result() const { return (cnt > 0) ? (sum / cnt) : 0.0; }
protected:
	int cnt;
	double sum;
};

/**
 * Return the arithmetic mean of a list of postive numbers
 */

template<typename ForwardIterator>
double mean(ForwardIterator first, ForwardIterator last)
{
	return std::for_each(first, last, IncrementalMean()).result();
}

} // namespace dominiqs

#endif /* MATHS_H */
