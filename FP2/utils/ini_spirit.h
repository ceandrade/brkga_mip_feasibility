/**
 * @file ini_spirit.h
 * @brief INI file reader/writer Header
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2008
 */

#ifndef INI_SPIRIT_H
#define INI_SPIRIT_H

#include <map>
#include <string>
#include <iosfwd>
#include <list>
#include <boost/shared_ptr.hpp>

namespace dominiqs {

struct Entry
{
	Entry(const std::string& n, const std::string& v = "");
	std::string name;
	std::string value;
};

inline Entry::Entry(const std::string& n, const std::string& v) : name(n), value(v) {}

typedef std::list<Entry> EntryList;

/* INI Category */
struct Category
{
public:
	std::string name;
	Category(const std::string& _name);
	void addEntry(const std::string& entry, const std::string& value);
	void deleteEntry(const std::string& entry);
	std::string getEntry(const std::string& entry, const std::string& defValue) const;
	// iteration
	typedef EntryList::const_iterator Iterator;
	Iterator begin() const;
	Iterator end() const;
protected:
	EntryList entries;
	friend std::ostream& operator<<(std::ostream&, const Category&);
};

typedef boost::shared_ptr<Category> CategoryPtr;

std::ostream& operator<<(std::ostream&, const Category&);

/* INI File Reader/Writer */

class IniFile
{
public:
	bool load(const std::string& filename);
	void save(std::ostream& out) const;
	void save(const std::string& filename) const;

	CategoryPtr addCategory(const std::string& name);
	void addCategory(CategoryPtr cat);
	CategoryPtr getCategory(const std::string& name) const;
	void deleteCategory(const std::string& name);

	std::string get(const std::string& cat, const std::string& entry, const std::string& defValue) const;
	void set(const std::string& cat, const std::string& entry, const std::string& value);
protected:
	typedef std::list<CategoryPtr> CategoryList;
	CategoryList categories;
};

typedef boost::shared_ptr<IniFile> IniFilePtr;

/**
 * Global Config File
 */

IniFile& gConfig2();

} // namespace dominiqs

#endif /* INI_SPIRIT_H */

