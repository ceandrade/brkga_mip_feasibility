/**
 * @file xmlconfig.h
 * @brief XML Config file reader/writer
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2012
 */

#ifndef XMLCONFIG_H
#define XMLCONFIG_H

#include <map>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>

namespace dominiqs {

/**
 * Config Category
 */

struct XMLCategory
{
public:
	std::string name;
	XMLCategory(const std::string& _name);
	void addEntry(const std::string& entry, const std::string& value);
	void deleteEntry(const std::string& entry);
	std::string getEntry(const std::string& entry, const std::string& defValue) const;
	typedef std::map<std::string, std::string> EntryMap;
	EntryMap entries;
};

typedef boost::shared_ptr<XMLCategory> XMLCategoryPtr;

/**
 * Config File Reader/Writer
 */

class XMLConfig
{
public:
	/** File I/O */
	//@{
	/**
	 * Load Config from an XML file
	 * @param fileName file to load
	 * @param merge merges with current contents if set to true, replaces otherwise
	 */
	bool load(const std::string& fileName, bool merge = true);
	/**
	 * Save configuration to an XML file
	 * @param fileName file to save
	 */
	void save(const std::string& fileName) const;
	//@}

	/** typed wrapper around get/set */
	//@{
	template<typename T> T get(const std::string& cat, const std::string& entry, const T& defValue) const
	{
		return boost::lexical_cast<T>(getAsString(cat, entry, boost::lexical_cast<std::string>(defValue)));
	}
	template<typename T> void set(const std::string& cat, const std::string& entry, const T& value)
	{
		setAsString(cat, entry, boost::lexical_cast<std::string>(value));
	}
	//@}
private:
	typedef std::map<std::string, XMLCategoryPtr> XMLCategoryMap;
	XMLCategoryMap categories;
	/** std::string get/set */
	//@{
	/**
	 * Get the value of a parameter
	 * @param @cat parameter category
	 * @param @entry parameter name
	 * @param @defValue default value if parameter not found
	 * @return the parameter value (or @param defValue if not found)
	 */
	std::string getAsString(const std::string& cat, const std::string& entry, const std::string& defValue) const;
	/**
	 * Set the value of a parameter. Will create one if it does not exists yet
	 * @param @cat parameter category
	 * @param @entry parameter name
	 * @param @value parameter value
	 */
	void setAsString(const std::string& cat, const std::string& entry, const std::string& value);
	//@}
};

/**
 * template specialization of get/set for std::string
 */

template<> std::string XMLConfig::get(const std::string& cat, const std::string& entry, const std::string& defValue) const;

template<> void XMLConfig::set(const std::string& cat, const std::string& entry, const std::string& value);

/**
 * Global Config File
 */

XMLConfig& gConfig();

} // namespace dominiqs

#endif /* XMLCONFIG_H */
