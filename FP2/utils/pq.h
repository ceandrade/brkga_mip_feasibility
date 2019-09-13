/**
 * @file pq.h
 * @brief Templatized Priority Queue
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2007
 */

#ifndef PQ_H
#define PQ_H

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include "asserter.h"

namespace dominiqs {

/**
 * Template Class implementing an highly customizable priority queue.
 * It stores objects of type T both by ID and by priority P.
 * @param T data type stored in the container - usually a (smart) pointer to something
 * @param ID ID type
 * @param ExtractID functor type to extract id from objects (key extractor). Must define result_type (=ID)
 * @param P priority type
 * @param ExtractP functor type to extract priorities from objects (key extractor). Must define result_type (=P)
 * @param CompareP functor type to compare priorities
 */

template<class T, class ID, class ExtractID, class P, class ExtractP, class CompareP>
class PriorityQueue
{
private:
	typedef boost::multi_index_container<
		T,
		boost::multi_index::indexed_by<
			boost::multi_index::hashed_unique< ExtractID >,
			boost::multi_index::ordered_non_unique< ExtractP, CompareP >
		>
	> Container;
	typedef typename Container::template nth_index<0>::type IndexByID;
	typedef typename Container::template nth_index<1>::type IndexByP;
	Container c;
public:
	/* iteration */
	typedef typename IndexByP::const_iterator const_iterator;
	const_iterator begin() const { return c.get<1>().begin(); }
	const_iterator end() const { return c.get<1>().end(); }
	/* size & clear */
	int size() const { return c.size(); }
	void clear() { c.clear(); }
	/* push object into the queue */
	void push(const T& t) { c.insert(t); }
	/* pop min object from the queue */
	T pop()
	{
		iterator res = begin();
		DOMINIQS_ASSERT(res != end());
		T tmp = *res;
		c.get<1>().erase(res);
		return tmp;
	}
	/* erase object of given id form the queue */
	void erase(const ID& id)
	{
		typename IndexByID::iterator itr = c.get<0>().find(id);
		if (itr != c.get<0>().end()) c.get<0>().erase(itr);
	}
};

} // namespace dominiqs

#endif /* PQ_H */
