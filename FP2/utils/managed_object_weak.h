/**
 * @file managed_object.h
 * @brief Smart Object with native support for smart pointers and virtual constructors
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2007-2008
 */

#ifndef MANAGED_OBJECT_WEAK_H
#define MANAGED_OBJECT_WEAK_H

#include "intrusive_weak_ptr.h"
#include "asserter.h"

#ifndef NDEBUG
#include <exception> //< for terminate
#include <iostream> //< for clog
#endif // NDEBUG

namespace dominiqs {

/* forward declarations */
class ManagedObjectWeak;
void intrusive_ptr_add_ref(ManagedObjectWeak* obj);
void intrusive_ptr_release(ManagedObjectWeak* obj);
intrusive_weak_ptr_base* intrusive_weak_ptr_get_head(ManagedObjectWeak* obj);
void intrusive_weak_ptr_set_head(ManagedObjectWeak* obj, intrusive_weak_ptr_base* ptr);

/**
 * @brief Managed Object Weak
 *
 * Classes derived from ManagedObjectWeak gracefully support smart pointers (intrusive, strong & weak)
 * Native support for virtual constructors (default & copy)
 * Be carefull not to pass a stack-allocated object to a smart pointer by the way
 * (this is a general rule, not restricted to this class ;-) )
 */

class ManagedObjectWeak
{
public:
	virtual ~ManagedObjectWeak()
	{
#ifndef NDEBUG
		if (ref_cnt != 0)
		{
			std::clog << "ManagedObjectWeak counter not zero in destructor: " << ref_cnt << std::endl;
			std::terminate();
		}
#endif // NDEBUG
		intrusive_weak_ptr<ManagedObjectWeak>::release(wptr_head);
	}
	/* virtual constructor idiom (cannot return smart pointer directly) */
	virtual ManagedObjectWeak* clone() const = 0;
protected:
	/* only heap objects! use virtual idiom for construction/cloning */
	ManagedObjectWeak() : wptr_head(0), ref_cnt(0) {}
private:
	intrusive_weak_ptr_base* wptr_head;
	friend intrusive_weak_ptr_base* intrusive_weak_ptr_get_head(ManagedObjectWeak* obj);
	friend void intrusive_weak_ptr_set_head(ManagedObjectWeak* obj, intrusive_weak_ptr_base* ptr);
	int ref_cnt;
	friend void intrusive_ptr_add_ref(ManagedObjectWeak* obj);
	friend void intrusive_ptr_release(ManagedObjectWeak* obj);
	/* only heap objects! use virtual idiom for construction/cloning */
	ManagedObjectWeak(const ManagedObjectWeak&);
	ManagedObjectWeak& operator=(const ManagedObjectWeak&);
};

/* inlines */
inline void intrusive_ptr_add_ref(ManagedObjectWeak* obj) { ++(obj->ref_cnt); }
inline void intrusive_ptr_release(ManagedObjectWeak* obj) { --(obj->ref_cnt); if (obj->ref_cnt == 0) delete obj; }
inline intrusive_weak_ptr_base* intrusive_weak_ptr_get_head(ManagedObjectWeak* obj) { return obj->wptr_head; }
inline void intrusive_weak_ptr_set_head(ManagedObjectWeak* obj, intrusive_weak_ptr_base* ptr) { obj->wptr_head = ptr; }

} // namespace dominiqs

#endif /* MANAGED_OBJECT_WEAK_H */
