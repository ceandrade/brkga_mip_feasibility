/**
 * @file str_utils.h
 * @brief String Utilities
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2011
 */

#ifndef STR_UTILS_H
#define STR_UTILS_H

#include <string>
#include <iterator>
#include <sstream>
#include <iostream>

namespace dominiqs {

/**
 * Escape character data, replacing &<>"' with the corresponding
 * XML entities
 */

template<typename InputIterator>
void xmlEscape(InputIterator first, InputIterator end, std::ostream& out)
{
	while (first != end)
	{
		char ch = *first++;
		if (ch == '&') out << "&amp;";
		else if (ch == '<') out << "&lt;";
		else if (ch == '>') out << "&gt;";
		else if (ch == '\'') out << "&apos;";
		else if (ch == '"') out << "&quot;";
		else out << ch;
	}
}

/**
 * Print Python-like unnested list and maps
 */

namespace details
{

/**
 * Type-dependent quote character
 */

template<typename T>
inline
const char* quote() { return ""; }

template<>
inline
const char* quote<const std::string>() { return "\'"; }

template<>
inline
const char* quote<std::string>() { return "\'"; }

template<>
inline
const char* quote<const char*>() { return "\'"; }

template<>
inline
const char* quote<char*>() { return "\'"; }

} // namespace details

/**
 * Print range [first,last) of a container as a Python-like unnested list
 * @param out output stream
 * @param first beginnning of the range
 * @param last end of the range
 * @param precision precision for numeric values
 * @param nl true to add a newline at the end, false (default) otherwise
 */

template<typename BidirectionalIter>
void printList(std::ostream& out,
	BidirectionalIter first, BidirectionalIter last,
	std::streamsize precision = 15, bool nl = false)
{
	if (first != last)
	{
		using namespace details;
		typedef typename std::iterator_traits<BidirectionalIter>::value_type VT;
		// set precision
		std::streamsize p = out.precision();
		out.precision(precision);
		// print list
		out << "[";
		BidirectionalIter beforeLast = last;
		beforeLast--;
		while (first != beforeLast)
		{
			out << quote<VT>() << *first << quote<VT>() << ", ";
			first++;
		}
		out << quote<VT>() << *first << quote<VT>() << "]";
		// restore precision
		out.precision(p);
	}
	else out << "[]";
	if (nl) out << std::endl;
}

/**
 * Convert range [first,last) of a container to a string with a Python-like syntax
 * @param first beginnning of the range
 * @param last end of the range
 * @param precision precision for numeric values
 * @return string encoding of the range
 */

template<typename BidirectionalIter>
std::string list2str(BidirectionalIter first, BidirectionalIter last,
	std::streamsize precision = 15)
{
	std::ostringstream os;
	printList(os, first, last, precision, false);
	return os.str();
}

/**
 * Print range [first,last) of an associative container as a Python-like map
 * @param out output stream
 * @param first beginnning of the range
 * @param last end of the range
 * @param precision precision for numeric values
 * @param nl true to add a newline at the end, false (default) otherwise
 */

template<typename BidirectionalIter>
void printMap(std::ostream& out,
	BidirectionalIter first, BidirectionalIter last,
	std::streamsize precision = 15, bool nl = false)
{
	if (first != last)
	{
		using namespace details;
		typedef typename std::iterator_traits<BidirectionalIter>::value_type PairType;
		typedef typename PairType::first_type KT;
		typedef typename PairType::second_type VT;
		// set precision
		std::streamsize p = out.precision();
		out.precision(precision);
		// print map
		out << "{";
		BidirectionalIter beforeLast = last;
		beforeLast--;
		while (first != beforeLast)
		{
			out << quote<KT>() << first->first << quote<KT>() << ": " << quote<VT>() << first->second << quote<VT>() << ", ";
			++first;
		}
		out << quote<KT>() << first->first << quote<KT>() << ": " << quote<VT>() << first->second << quote<VT>() << "}";
		// restore precision
		out.precision(p);
	}
	else out << "{}";
	if (nl) out << std::endl;
}

} // namespace dominiqs

#endif /* STR_UTILS_H */
