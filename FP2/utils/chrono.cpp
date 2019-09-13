/**
 * @file chrono.cpp
 * @brief CPU Stopwatch for benchmarks
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2007
 */

#include <ctime>
#include <cstdlib>
#include <sys/resource.h>
#ifdef __APPLE__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include "chrono.h"

namespace dominiqs {

void Chrono::CpuTime(double& user_time, double& sys_time)
{
	static struct rusage usage;
	int res = getrusage(RUSAGE_SELF, &usage);
	if (res) return;
	// process time
	user_time = usage.ru_utime.tv_sec;
	user_time += 1.0e-6*((double) usage.ru_utime.tv_usec);
	// OS time on behalf of process
	sys_time = usage.ru_stime.tv_sec;
	sys_time += 1.0e-6*((double) usage.ru_stime.tv_usec);
}

void Chrono::WallTime(double& time)
{
	static timespec ts;
#ifdef __APPLE__
	static clock_serv_t cclock;
	static mach_timespec_t mts;
	host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);
	ts.tv_sec = mts.tv_sec;
	ts.tv_nsec = mts.tv_nsec;
#else
	clock_gettime(CLOCK_MONOTONIC, &ts);
#endif // __APPLE__
	time = ts.tv_sec;
	time += 1.0e-9*((double) ts.tv_nsec);
}

Chrono& gChrono()
{
	static Chrono theChrono;
	return theChrono;
}

} // namespace dominiqs

