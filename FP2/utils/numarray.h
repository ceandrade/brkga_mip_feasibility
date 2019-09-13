/**
 * @file numarray.h
 * @brief Dense Vector for numerical data
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * Copyright 2011-2012 Domenico Salvagnin
 */

#ifndef NUMARRAY_H
#define NUMARRAY_H

#include "asserter.h"
#include "machine_utils.h"
#include "maths.h"
#include <cstring>

namespace dominiqs
{

/**
 * @brief Vector replacement for numerical data.
 *
 * It has the following "improvements" over std::vectoc:
 * - allocated always memory is properly aligned (enabling SSE2 instructions)
 * - no constructor/destructor calls for elements
 * - fast push_back without size checks (push_back_unsafe method)
 * - explicit conversion to C array (via c_ptr() method)
 *
 * @tparam T value type of the numarray
 */

template<typename T>
class numarray
{
public:
	// typedefs
	//@{
	typedef T value_type;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef ptrdiff_t difference_type;
	typedef size_t size_type;
	typedef T* iterator;
	typedef const T* const_iterator;
	//@}

	/**
	 * Create an empty numarray
	 */
	numarray() : data(0), length(0), alloc(0) {}

	/**
	 * Create a numarray with n uninitialized elements.
	 */
	numarray(size_type n)
	{
		if (n)
		{
			data = static_cast<T*>(mallocSSE2(n * sizeof(T)));
			DOMINIQS_ASSERT( data );
			length = n;
			alloc = n;
		}
		else
		{
			data = 0;
			length = 0;
			alloc = 0;
		}
	}

	/**
	 * Copy constructor
	 */
	numarray(const numarray& other)
	{
		if (other.length)
		{
			data = static_cast<T*>(mallocSSE2(other.length * sizeof(T)));
			DOMINIQS_ASSERT( data );
			std::memcpy(data, other.data, other.length * sizeof(T));
			length = other.length;
			alloc = other.length;
		}
		else
		{
			data = 0;
			length = 0;
			alloc = 0;
		}
	}

	/**
	 * assignment operator (swap idiom)
	 */
	numarray& operator=(const numarray& other)
	{
		if (this != &other)
		{
			numarray temp(other);
			swap(temp);
		}
		return *this;
	}

	~numarray() throw()
	{
		freeSSE2(data);
	}

	void swap(numarray& other) throw()
	{
		std::swap(data, other.data);
		std::swap(length, other.length);
		std::swap(alloc, other.alloc);
	}
	// size management methods
	//@{
	inline size_type size() const { return length; }
	inline size_type capacity() const { return alloc; }
	/**
	 * Resize the array
	 *
	 * Memory is reallocated only if necessary.
	 * In any case, new elements are uninitialized
	 *
	 * @param newSize new size
	 */
	void resize(size_type newSize)
	{
		if (newSize)
		{
			reserve(newSize);
			length = newSize;
		}
		else
		{
			freeSSE2(data);
			data = 0;
			length = 0;
			alloc = 0;
		}
	}
	inline bool empty() const { return (length == 0); }
	void reserve(size_type n)
	{
		if (n > alloc)
		{
			numarray temp(n);
			std::memcpy(temp.data, data, length * sizeof(T));
			temp.length = length;
			swap(temp);
		}
	}
	inline void clear() { length = 0; }
	//@}

	// push methods
	//@{
	/**
	 * Push a new element to the end of array.
	 *
	 * This method checks if there is enough memory for the operation, if not it asks for more.
	 *
	 * @param val value to add
	 */
	inline void push_back(const value_type& val)
	{
		if (length == alloc) reserve(std::max(2 * alloc, (size_type)8));
		push_back_unsafe(val);
	}
	/**
	 * Push a new element to the end of array.
	 *
	 * This method does NOT check if there is enough memory for the operation.
	 *
	 * @param val value to add
	 */
	inline void push_back_unsafe(const value_type& val)
	{
		data[length++] = val;
	}
	//@}

	// access methods
	//@{
	inline reference operator[](size_type i) { return data[i]; }
	inline const_reference operator[](size_type i) const { return data[i]; }
	inline reference front()
	{
		DOMINIQS_ASSERT( length );
		return data[0];
	}
	inline const_reference front() const
	{
		DOMINIQS_ASSERT( length );
		return data[0];
	}
	inline reference back()
	{
		DOMINIQS_ASSERT( length );
		return data[length - 1];
	}
	inline const_reference back() const
	{
		DOMINIQS_ASSERT( length );
		return data[length - 1];
	}
	inline pointer c_ptr() { return data; }
	inline const_pointer c_ptr() const { return data; }
	//@}

	// iterators
	//@{
	inline iterator begin() { return data; }
	inline const_iterator begin() const { return data; }
	inline iterator end() { return (data + length); }
	inline const_iterator end() const { return (data + length); }
	//@}

	/**
	 * Assign a given value to every element in the array
	 *
	 * @param val value to assign
	 */
	void fill(const value_type& val)
	{
		for (size_type i = 0; i < length; ++i) data[i] = val;
	}
	/**
	 * Assign the value zero to every element in the array
	 */
	void zero()
	{
		if (data) std::memset((void*)data, 0, length * sizeof(T));
	}
	/**
	 * Initialize the array with consecutive elements, starting from @param value
	 */
	void iota(const value_type& value)
	{
		if (data) dominiqs::iota(data, data + length, value);
	}
private:
	T* data;
	size_type length;
	size_type alloc;
};

} // namespace

#endif /** NUMARRAY_H */
