/**
 * @file cpxmacro.h
 * Cplex Helper Macros
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2007-2012
 */

#ifndef CPX_MACRO_H
#define CPX_MACRO_H

#include <cstring>
#include <string>

namespace dominiqs {

#define STRINGIZE(something) STRINGIZE_HELPER(something)
#define STRINGIZE_HELPER(something) #something

/* Cplex Error Status and Message Buffer */

extern int status;

const unsigned int BUF_SIZE = 4096;

extern char errmsg[BUF_SIZE];

/* Shortcut for declaring a Cplex Env */
#define DECL_ENV(name) \
CPXENVptr name = CPXopenCPLEX(&dominiqs::status);\
do { \
if (status){\
	CPXgeterrorstring(NULL, status, errmsg);\
	int trailer = std::strlen(errmsg) - 1;\
	if (trailer >= 0) errmsg[trailer] = '\0';\
	throw std::runtime_error(std::string(__FILE__) + ":" + STRINGIZE(__LINE__) + ": " + errmsg);\
} \
} while(false)

/* Shortcut for initializing a Cplex Env */
#define INIT_ENV(name) \
name = CPXopenCPLEX(&status);\
do { \
if (status){\
	CPXgeterrorstring(NULL, status, errmsg);\
	int trailer = std::strlen(errmsg) - 1;\
	if (trailer >= 0) errmsg[trailer] = '\0';\
	throw std::runtime_error(std::string(__FILE__) + ":" + STRINGIZE(__LINE__) + ": " + errmsg);\
} \
} while(false)

/* Shortcut for freeing a Cplex Env (does not throw if errors occur)*/
#define FREE_ENV(name) CPXcloseCPLEX(&name)

/* Shortcut for declaring a Cplex Problem */
#define DECL_PROB(env, name) \
CPXLPptr name = CPXcreateprob(env, &status, "");\
do { \
if (status){\
	CPXgeterrorstring(NULL, status, errmsg);\
	int trailer = std::strlen(errmsg) - 1;\
	if (trailer >= 0) errmsg[trailer] = '\0';\
	throw std::runtime_error(std::string(__FILE__) + ":" + STRINGIZE(__LINE__) + ": " + errmsg);\
} \
} while(false)

/* Shortcut for initializing a Cplex Problem */
#define INIT_PROB(env, name) \
name = CPXcreateprob(env, &status, "");\
do { \
if (status){\
	CPXgeterrorstring(NULL, status, errmsg);\
	int trailer = std::strlen(errmsg) - 1;\
	if (trailer >= 0) errmsg[trailer] = '\0';\
	throw std::runtime_error(std::string(__FILE__) + ":" + STRINGIZE(__LINE__) + ": " + errmsg);\
} \
} while(false)

/* Shortcut for freeing a Cplex Problem (does not throw if errors occur)*/
#define FREE_PROB(env, name) CPXfreeprob(env, &(name))

#define CLONE_PROB(env, oldlp, newlp) \
newlp = CPXcloneprob(env, oldlp, &status);\
do {\
if (status){\
	CPXgeterrorstring(NULL, status, errmsg);\
	int trailer = std::strlen(errmsg) - 1;\
	if (trailer >= 0) errmsg[trailer] = '\0';\
	throw std::runtime_error(std::string(__FILE__) + ":" + STRINGIZE(__LINE__) + ": " + errmsg);\
} \
} while(false)

/* Make a checked call to a Cplex API function */
#define CHECKED_CPX_CALL(func, env, ...) do {\
status = func(env, __VA_ARGS__);\
if (status){\
	CPXgeterrorstring(env, status, errmsg);\
	int trailer = std::strlen(errmsg) - 1;\
	if (trailer >= 0) errmsg[trailer] = '\0';\
	throw std::runtime_error(std::string(__FILE__) + ":" + STRINGIZE(__LINE__) + ": " + errmsg);\
} \
} while(false)

} // namespace dominiqs

#endif /* CPX_MACRO_H */

