/**
* @file xmlconfig.cpp
* @brief XML Config file reader/writer
*
* @author Domenico Salvagnin dominiqs@gmail.com
* 2012
*/

#include "xmlconfig.h"
#include "myxml.h"
#include "asserter.h"

namespace dominiqs {

/* Config XMLCategory */

XMLCategory::XMLCategory(const std::string& _name) : name(_name) {}

void XMLCategory::addEntry(const std::string& entry, const std::string& value)
{
	entries[entry] = value;
}

void XMLCategory::deleteEntry(const std::string& entry)
{
	entries.erase(entry);
}

std::string XMLCategory::getEntry(const std::string& entry, const std::string& defValue) const
{
	EntryMap::const_iterator itr = entries.find(entry);
	if (itr == entries.end()) return defValue;
	return itr->second;
}

bool XMLConfig::load(const std::string& filename, bool merge)
{
	if (!merge) categories.clear();

	XmlDoc doc;
	doc.read(filename);
	std::vector<XmlNode> cats;
	doc.xpath("/config/category", cats);

	for (XmlNode cat: cats)
	{
		std::string catName = cat.attr("name");
		XMLCategoryMap::iterator itr = categories.find(catName);
		if (itr == categories.end())
		{
			itr = categories.insert(XMLCategoryMap::value_type(catName, XMLCategoryPtr(new XMLCategory(catName)))).first;
			DOMINIQS_ASSERT( itr != categories.end() );
		}
		XMLCategoryPtr catPtr = itr->second;
		XmlNode entry = cat.firstChild();
		while(!entry.isNull())
		{
			catPtr->addEntry(entry.name(), entry.data());
			entry = entry.next();
		}
	}
	return true;
}

void XMLConfig::save(const std::string& filename) const
{
	XmlDoc doc;
	XmlNode root = doc.setRoot("config");
	for (const auto& catv: categories)
	{
		XMLCategoryPtr cat = catv.second;
		AttributeMap attrs;
		attrs["name"] = cat->name;
		XmlNode catNode = root.addChild("category", attrs);
		for (const auto& entry: cat->entries)
		{
			catNode.addChild(entry.first, entry.second);
		}
	}
	doc.write(filename);
}

std::string XMLConfig::getAsString(const std::string& cat, const std::string& entry, const std::string& defValue) const
{
	XMLCategoryMap::const_iterator catItr = categories.find(cat);
	if (catItr == categories.end()) return defValue;
	return catItr->second->getEntry(entry, defValue);
}

void XMLConfig::setAsString(const std::string& cat, const std::string& entry, const std::string& value)
{
	XMLCategoryMap::iterator itr = categories.find(cat);
	if (itr == categories.end())
	{
		itr = categories.insert(XMLCategoryMap::value_type(cat, XMLCategoryPtr(new XMLCategory(cat)))).first;
		DOMINIQS_ASSERT( itr != categories.end() );
	}
	itr->second->entries[entry] = value;
}

template<> std::string XMLConfig::get(const std::string& cat, const std::string& entry, const std::string& defValue) const
{
	return getAsString(cat, entry, defValue);
}

template<> void XMLConfig::set(const std::string& cat, const std::string& entry, const std::string& value)
{
	setAsString(cat, entry, value);
}

XMLConfig& gConfig()
{
	static XMLConfig theFile;
	return theFile;
}

} // namespace dominiqs
