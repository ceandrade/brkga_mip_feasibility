/**
 * @file machine_utils.h
 * @brief Machine utilities
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * Copyright 2009-2012 Domenico Salvagnin
 */

#ifndef MACHINE_UTILS_H
#define MACHINE_UTILS_H

#include <stddef.h> //< for size_t and ptrdiff_t
#include <unistd.h> //< for sysconf
#ifdef __APPLE__
#include <sys/sysctl.h>
#else
#include <emmintrin.h>
#endif // __APPLE__

#ifndef NDEBUG
#include <exception> //< for terminate
#include <iostream> //< for clog
#endif // NDEBUG

#include "asserter.h"

namespace dominiqs {

/**
 * Allocate @param size bytes of memory
 * The returned block is guaranteed to be 16-aligned (to play nicely with SSE2)
 * @return a pointer to the allocated block
 */

inline void* mallocSSE2(std::size_t size)
{
	void* result;
	#ifdef __APPLE__
		result = std::malloc(size);
	#else
		result = _mm_malloc(size, 16);
	#endif
	DOMINIQS_ASSERT( (reinterpret_cast<ptrdiff_t>(result) % 16) == 0 );
	return result;
}

/**
 * Free a 16-aligned memmory block allocated by mallocSSE2
 */

inline void freeSSE2(void* ptr)
{
#ifndef NDEBUG
	if ((reinterpret_cast<ptrdiff_t>(ptr) % 16) != 0)
	{
		std::clog << "freeSSE2 called with bad pointer address: " << ptr << std::endl;
		std::terminate();
	}
#endif // NDEBUG
	#ifdef __APPLE__
		std::free(ptr);
	#else
		_mm_free(ptr);
	#endif
}

/**
 * Simple scoped_ptr to automatically deallocate memory allocated with mallocSSE2
 * scoped_ptr_sse2 is NOT copyable
 * Users can retake ownership of allocated memory by calling release()
 */
template<class T>
class scoped_ptr_sse2
{
public:
	typedef T element_type;
	explicit scoped_ptr_sse2(T* p = 0) throw() : ptr(p) {}
	~scoped_ptr_sse2() throw() { freeSSE2(ptr); }

	inline T& operator*() const throw() { return *ptr; }
	inline T* operator->() const throw() { return ptr; }
	inline T* get() const throw() { return ptr; }
	inline T* release() throw()
	{
		T* t = ptr;
		ptr = 0;
		return t;
	}
private:
	T* ptr;
	scoped_ptr_sse2(const scoped_ptr_sse2& other);
	scoped_ptr_sse2& operator=(const scoped_ptr_sse2& other);
};

/**
 * Return the amount of physical memory of the system in bytes
 */

inline double getPhysicalMemory()
{
#ifdef __APPLE__
	uint64_t memsize;
	size_t len = sizeof(memsize);
	int mib[2] = { CTL_HW, HW_MEMSIZE };
	if ((sysctl(mib, 2, &memsize, &len, 0, 0) == 0) && (len == sizeof(memsize))) return (double)memsize;
	return 0;
#else
	double res = sysconf(_SC_PAGESIZE) * (double)sysconf(_SC_PHYS_PAGES);
	if (res >= 0) return res;
	return 0;
#endif // __APPLE__
}

/**
 * Return the amount of available memory of the system in bytes
 */

inline double getAvailableMemory()
{
#ifdef __APPLE__
	int memsize;
	size_t len = sizeof(memsize);
	int mib[2] = { CTL_HW, HW_USERMEM };
	if ((sysctl(mib, 2, &memsize, &len, 0, 0) == 0) && (len == sizeof(memsize))) return (double)memsize;
	return 0;
#else
	double res = sysconf(_SC_PAGESIZE) * (double)sysconf(_SC_AVPHYS_PAGES);
	if (res >= 0) return res;
	return 0;
#endif // __APPLE__
}

/**
 * Return the number of physical cores in the system
 * This number may be the same as the number of logical cores,
 * depending on hyper-threading.
 * The returned number is accured on Mac platforms (less so on
 * Linux, where we resort to sysconf)
 */

inline int getNumPhysicalCores()
{
#ifdef __APPLE__
	int physicalCores;
	size_t len = sizeof(physicalCores);
	sysctlbyname("hw.physicalcpu", &physicalCores, &len, 0, 0);
	return physicalCores;
#else
	return sysconf(_SC_NPROCESSORS_ONLN);
#endif // __APPLE__
}

/**
 * Return the number of logical cores in the system
 */

inline int getNumLogicalCores()
{
	return sysconf(_SC_NPROCESSORS_ONLN);
}

} // namespace dominiqs

#endif /* MACHINE_UTILS_H */
