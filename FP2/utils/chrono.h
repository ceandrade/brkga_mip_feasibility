/**
 * @file chrono.h
 * @brief CPU Stopwatch for benchmarks
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2007
 */

#ifndef CHRONO_H
#define CHRONO_H

#include "asserter.h"

namespace dominiqs {

/**
 * Benchmarking class
 * Simulates a simple stopwatch
 * Measures the CPU time used by a process and the wall clock time
 */

class Chrono
{
public:
	enum ClockType { CPU_TIME, WALL_CLOCK };
	/** default constructor */
	Chrono(bool autoStart = false);
	/** start stopwatch */
	void start();
	/** stop stopwatch */
	void stop();
	/** reset stopwatch */
	void reset();

	/** get/set default clock type */
	ClockType getDefaultType() const;
	void setDefaultType(ClockType type);

	/** @return partial elapsed time in seconds (according to default type)*/
	double getPartial() const;
	/** @return total elapsed time in seconds (according to default type)*/
	double getTotal() const;
	/** @return elapsed time in seconds (according to default type)*/
	double getElapsed() const;

	/** @return partial elapsed CPU time in seconds*/
	double getCPUPartial() const;
	/** @return total elapsed CPU time in seconds*/
	double getCPUTotal() const;
	/** @return elapsed CPU time in seconds*/
	double getCPUElapsed() const;

	/** @return partial elapsed wall clock time in seconds*/
	double getWallPartial() const;
	/** @return total elapsed wall clock time in seconds*/
	double getWallTotal() const;
	/** @return elapsed wall clock time in seconds*/
	double getWallElapsed() const;
private:
	// CPU time group @{
	// user time
	double u_begin, u_end, u_total;
	// system time
	double s_begin, s_end, s_total;
	static void CpuTime(double& user_time, double& sys_time);
	// @}

	// Wall clock time group @{
	double w_begin, w_end, w_total;
	static void WallTime(double& time);
	// @}
	
	ClockType defaultTime;
};

inline Chrono::Chrono(bool autoStart)
	:  u_begin(0.0), u_end(0.0), u_total(0.0),
	s_begin(0.0), s_end(0.0), s_total(0.0),
	w_begin(0.0), w_end(0.0), w_total(0.0),
	defaultTime(CPU_TIME)
{
	if (autoStart) start();
}

inline void Chrono::start()
{
	CpuTime(u_begin, s_begin);
	WallTime(w_begin);
}

inline void Chrono::stop()
{
	CpuTime(u_end, s_end);
	WallTime(w_end);
	u_total += (u_end - u_begin);
	s_total += (s_end - s_begin);
	w_total += (w_end - w_begin);
}

inline void Chrono::reset()
{
	u_begin = 0.0;
	u_end = 0.0;
	u_total = 0.0;
	s_begin = 0.0;
	s_end = 0.0;
	s_total = 0.0;
	w_begin = 0.0;
	w_end = 0.0;
	w_total = 0.0;
}

inline Chrono::ClockType Chrono::getDefaultType() const
{
	return defaultTime;
}

inline void Chrono::setDefaultType(ClockType type)
{
	defaultTime = type;
}

inline double Chrono::getPartial() const
{
	if (defaultTime == CPU_TIME) return getCPUPartial();
	return getWallPartial();
}

inline double Chrono::getTotal() const
{
	if (defaultTime == CPU_TIME) return getCPUTotal();
	return getWallTotal();
}

inline double Chrono::getElapsed() const
{
	if (defaultTime == CPU_TIME) return getCPUElapsed();
	return getWallElapsed();
}

inline double Chrono::getCPUPartial() const
{
	return (u_end - u_begin) + (s_end - s_begin);
}

inline double Chrono::getCPUTotal() const
{
	return u_total + s_total;
}

inline double Chrono::getCPUElapsed() const
{
	static double u_now, s_now;
	CpuTime(u_now, s_now);
	return (u_now - u_begin) + (s_now - s_begin);
}

inline double Chrono::getWallPartial() const
{
	return (w_end - w_begin);
}

inline double Chrono::getWallTotal() const
{
	return w_total;
}

inline double Chrono::getWallElapsed() const
{
	static double w_now;
	WallTime(w_now);
	return (w_now - w_begin);
}

/**
 * Global Stopwatch
 */

Chrono& gChrono();

/**
 * Automatic stopwatch stopper (RAII principle)
 */

class AutoChrono
{
public:
	AutoChrono(Chrono* c) : theChrono(c), pending(true)
	{
		DOMINIQS_ASSERT( theChrono );
		theChrono->start();
	}
	~AutoChrono() { stop(); }
	void stop()
	{
		if (pending)
		{
			theChrono->stop();
			pending = false;
		}
	}
private:
	Chrono* theChrono;
	bool pending;
};

/**
 * Automatic global stopwatch stopper (RAII principle)
 */

class GlobalAutoChrono
{
public:
	GlobalAutoChrono() : pending(true) { gChrono().start(); }
	~GlobalAutoChrono() { stop(); }
	void stop()
	{
		if (pending)
		{
			gChrono().stop();
			pending = false;
		}
	}
private:
	bool pending;
};

} // namespace dominiqs

#endif /* CHRONO_H */
