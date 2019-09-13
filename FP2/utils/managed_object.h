/**
 * @file managed_object.h
 * @brief Heap Only Object with native support for smart pointers
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2011
 */

#ifndef MANAGED_OBJECT_H
#define MANAGED_OBJECT_H

#include "asserter.h"

#ifndef NDEBUG
#include <exception> //< for terminate
#include <iostream> //< for clog
#endif // NDEBUG

namespace dominiqs {

/**
 * @brief Managed Object
 *
 * Classes derived from ManagedObject gracefully support intrusive smart pointers.
 * It does not however support weak intrusive smart pointers (@see ManagedObjectWeak for that!).
 * Moreover, it does not support non-intrusive pointers.
 * Managed objects are heap only objects!
 * A derived object Derived must be declared as class Derived : private ManagedObject<Derived>...
 */

template<class Derived>
class ManagedObject
{
public:
	friend inline void intrusive_ptr_add_ref(const Derived* obj)
	{
		DOMINIQS_ASSERT( obj );
		++(((const ManagedObject*)obj)->ref_cnt);
	}
	friend inline void intrusive_ptr_release(const Derived* obj)
	{
		DOMINIQS_ASSERT( obj );
		if (--(((const ManagedObject*)obj)->ref_cnt) == 0) delete obj;
	}
protected:
	// default/copy constructors
	ManagedObject() : ref_cnt(0) {}
	ManagedObject(const ManagedObject&) : ref_cnt(0) {}
	ManagedObject& operator=(const ManagedObject&) {}
	// virtual destructor
	virtual ~ManagedObject()
	{
#ifndef NDEBUG
		if (ref_cnt != 0)
		{
			std::clog << "ManagedObject counter not zero in destructor: " << ref_cnt << std::endl;
			std::terminate();
		}
#endif // NDEBUG
	}
	// swap operator
	void swap(ManagedObject&) {}
	// for debug only
	// unsigned long getCounter() const { return ref_cnt; }
private:
	mutable unsigned long ref_cnt;
};

} // namespace dominiqs

#endif /* MANAGED_OBJECT_H */
