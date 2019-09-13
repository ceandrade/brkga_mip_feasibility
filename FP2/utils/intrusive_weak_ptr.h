/**
 * @file intrusive_weak_ptr.h
 * @brief Intrusive Weak Ptr (Boost lacks this one...)
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2008
 */

#ifndef INTRUSIVE_WEAK_PTR_H
#define INTRUSIVE_WEAK_PTR_H

#include <boost/intrusive_ptr.hpp>

namespace dominiqs {

/**
 * Base Class to handle weak pointers list
 */

struct intrusive_weak_ptr_base
{
public:
	intrusive_weak_ptr_base() : next(0), prev(0) {}
	virtual ~intrusive_weak_ptr_base() {}
	intrusive_weak_ptr_base* next;
	intrusive_weak_ptr_base* prev;
	virtual void reset() =0;
	void insertIntoList(intrusive_weak_ptr_base* head)
	{
		next = head;
		if (head) head->prev = this;
		prev = 0;
	}
	void removeFromList()
	{
		if (prev) prev->next = next;
		if (next) next->prev = prev;
	}
};

/**
 * @brief Real intrusive weak pointer
 *
 * The user must provide 4 functions for cooperation between object (type T) and pointer (that is what intrusive means...)
 *
 * void intrusive_ptr_add_ref(T* obj): increases reference count of obj
 * void intrusive_ptr_release(T* obj): decreases reference count and eventually delete obj
 * intrusive_weak_ptr_base* intrusive_weak_ptr_get_head(T* obj): get list head of obj
 * void intrusive_weak_ptr_set_head(T* obj, intrusive_weak_ptr_base* ptr): set list head of obj
 *
 * (each object keeps a list of weak ptrs to itself and, in the destructor, calls intrusive_weak_ptr<T>::release(head) to reset pointers)
 */

template<class T>
class intrusive_weak_ptr : private intrusive_weak_ptr_base
{
public:
	intrusive_weak_ptr(T* o = 0) : obj(o)
	{
		insertIntoList(obj);
	}
	template<class U> intrusive_weak_ptr(intrusive_weak_ptr<U>& rhs)
	{
		obj = rhs.obj;
		insertIntoList(obj);
	}
	template<class U> intrusive_weak_ptr(boost::intrusive_ptr<U>& rhs)
	{
		obj = rhs.get();
		insertIntoList(obj);
	}
	template<class U> intrusive_weak_ptr& operator=(intrusive_weak_ptr<U>& rhs)
	{
		if (this != &rhs)
		{
			removeFromList(obj);
			obj = rhs.obj;
			insertIntoList(obj);
		}
		return *this;
	}
	template<class U> intrusive_weak_ptr& operator=(boost::intrusive_ptr<U>& rhs)
	{
		removeFromList(obj);
		obj = rhs.obj;
		insertIntoList(obj);
		return *this;
	}
	~intrusive_weak_ptr()
	{
		removeFromList(obj);
	}
	T* get() const // never throws
	{
		return obj;
	}
	T* operator->() const // never throws
	{
		return obj;
	}
	bool expired() const // never throws
    {
        return (obj == 0);
    }
	operator bool() const // never throws
    {
        return (obj != 0);
    }
	void reset() // never throws
	{
		obj = 0;
	}
	static void release(intrusive_weak_ptr_base* head)
	{
		while (head)
		{
			head->reset();
			head = head->next;
		}
	}
	T* obj;
protected:
	void insertIntoList(T* obj)
	{
		if (!obj) return;
		intrusive_weak_ptr_base::insertIntoList(intrusive_weak_ptr_get_head(obj));
		intrusive_weak_ptr_set_head(obj, this);
	}
	void removeFromList(T* obj)
	{
		if (!obj) return;
		intrusive_weak_ptr_base::removeFromList();
		if (!prev) intrusive_weak_ptr_set_head(obj, next);
	}
};

template<class T, class U> inline bool operator==(const intrusive_weak_ptr<T>& a, const intrusive_weak_ptr<U>& b)
{
	return a.get() == b.get();
}

template<class T, class U> inline bool operator!=(const intrusive_weak_ptr<T>& a, const intrusive_weak_ptr<U>& b)
{
	return a.get() != b.get();
}

template<class T, class U> inline bool operator<(const intrusive_weak_ptr<T>& a, const intrusive_weak_ptr<U>& b)
{
	return a.get() < b.get();
}

template<class T, class U> intrusive_weak_ptr<T> static_pointer_cast(const intrusive_weak_ptr<U>& p)
{
	return intrusive_weak_ptr<T>(static_cast<T*>(p.get()));
}

template<class T, class U> intrusive_weak_ptr<T> dynamic_pointer_cast(const intrusive_weak_ptr<U>& p)
{
	return intrusive_weak_ptr<T>(dynamic_cast<T*>(p.get()));
}

} // namespace dominiqs

#endif /* INTRUSIVE_WEAK_PTR_H */
