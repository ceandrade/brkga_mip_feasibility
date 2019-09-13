/**
 * @file asserter.h
 * @brief assert utilities
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2008
 */

#ifndef ASSERTER_H
#define ASSERTER_H

#include <stdexcept>

#define STRINGIZE(something) STRINGIZE_HELPER(something)
#define STRINGIZE_HELPER(something) #something

#ifndef NDEBUG
#define DOMINIQS_ASSERT(condition) do { if (!(condition)) throw std::logic_error(__FILE__ ":" STRINGIZE(__LINE__) ": assertion '" #condition "' failed"); } while(false)
#define DOMINIQS_ASSERT_EQUAL(first, second) do { if (!(first == second)) throw std::logic_error(__FILE__ ":" STRINGIZE(__LINE__) ": assertion '" #first " == " #second "' failed"); } while(false)
#else
#define DOMINIQS_ASSERT(unused) do {} while(false)
#define DOMINIQS_ASSERT_EQUAL(unused1, unused2) do {} while(false)
#endif

#endif /* ASSERTER_H */
