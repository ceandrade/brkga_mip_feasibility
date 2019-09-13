/**
 * @file factory.h
 * @brief Factory Pattern
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2013
 */

#ifndef FACTORY_H
#define FACTORY_H

#include <map>

namespace dominiqs {

template<typename BaseClassType, typename ClassType, typename... Args>
BaseClassType* Creator(Args... args)
{
	return new ClassType(args...);
}

template<typename BaseClassType, typename UniqueIdType, typename... Args>
class Factory
{
private:
	typedef BaseClassType* (*CreatorType)(Args...);
	typedef ::std::map<UniqueIdType, CreatorType> CMap;
public:
	/**
	 * Register class ClassType with name id
	 */
	template<typename ClassType>
	bool registerClass(UniqueIdType id)
	{
		if (m.find(id) != m.end()) return false;
		m[id] = &Creator<BaseClassType, ClassType, Args...>;
		return true;
	}
	/**
	 * Unregister class with name id
	 */
	bool unregisterClass(UniqueIdType id)
	{
		return (m.erase(id) == 1);
	}
	/**
	 * Instantiate a class of name id
	 */
	BaseClassType* create(UniqueIdType id, Args... args)
	{
		typename CMap::iterator iter = m.find(id);
		if (iter == m.end()) return 0;
		return ((*iter).second)(args...);
	}
	/**
	 * Returns the lists of currently registered names (ids)
	 */
	template<typename OutputIterator>
	void getIDs(OutputIterator out) const
	{
		typename CMap::const_iterator iter = m.begin();
		while (iter != m.end()) *out++ = (*iter++).first;
	}
protected:
	CMap m;
};

} // namespace dominiqs

#endif // FACTORY_H

