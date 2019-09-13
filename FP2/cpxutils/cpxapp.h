/**
 * @file cpxapp.h
 * @brief Basic Cplex Application
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * Copyright 2011 Domenico Salvagnin
 */

#ifndef CPXAPP_H
#define CPXAPP_H

#include "cpxutils.h"
#include <utils/app.h>

namespace dominiqs
{

/**
 * @brief Provides a basic Cplex commmand-line application
 * It adds to the basic App the following Cplex-related features:
 * - env/problem instances management
 * - read common params (time limit, presolve, etc...)
 * - read problem from file
 * - (optional) presolve
 * Users msut derived from this class to implement custom behaviour.
 */

class CPXApp : public App
{
public:
	CPXApp();
	/** destructor */
	virtual ~CPXApp() {}
	/**
	 * @brief read config params: to be implemented in derived classes
	 * to be extended in derived classes (optional)
	 */
	virtual void readConfig();
protected:
	/**
	 * @brief additional startup work
	 * create env / lp objects presolve
	 * to be extended in derived classes (optional)
	 */
	virtual void startup();
	/**
	 * @brief additional shutdown work
	 * to be extended in derived classes (optional)
	 */
	virtual void shutdown();
	/* data */
	Env env;
	Prob lp;
	std::string probName;
	std::string runName;
	bool presolve;
	double timeLimit;
	double linearEPS;
	int numThreads;
	double preOffset;
};

} // namespace dominiqs

#endif /* CPXAPP_H */
