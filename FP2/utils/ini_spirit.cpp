/**
 * @file ini_spirit.cpp
 * @brief INI file reader/writer Source
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2008
 */

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <boost/spirit/include/classic_core.hpp>
#include <boost/spirit/include/classic_confix.hpp>
#include <boost/spirit/include/classic_file_iterator.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/bind.hpp>

#include "ini_spirit.h"
#include "asserter.h"

using namespace boost::spirit::classic;
using boost::algorithm::trim;

namespace dominiqs {

typedef char                    char_t;
typedef file_iterator <char_t>  iterator_t;
typedef scanner<iterator_t>     scanner_t;
typedef rule <scanner_t>        rule_t;

class IniParserContext
{
public:
	IniParserContext(IniFile* _data);
	void addCategory(iterator_t first, iterator_t last) const;
	void addEntry(iterator_t first, iterator_t last) const;
	void addValue(iterator_t first, iterator_t last) const;
protected:
	IniFile* data;
	mutable std::string currentCat;
	mutable std::string currentEntry;
};

IniParserContext::IniParserContext(IniFile* _data) : data(_data) {}

void IniParserContext::addCategory(iterator_t first, iterator_t last) const
{
	currentCat = std::string(first, last);
	trim(currentCat);
	data->addCategory(currentCat);
}

void IniParserContext::addEntry(iterator_t first, iterator_t last) const
{
	currentEntry = std::string(first, last);
	trim(currentEntry);
	if (currentCat == "") // must be global entry
	{
		if (!data->getCategory("Globals")) data->addCategory("Globals");
		currentCat = "Globals";
	}
}

void IniParserContext::addValue(iterator_t first, iterator_t last) const
{
	std::string value(first, last);
	trim(value);
	data->set(currentCat, currentEntry, value);
}

Category::Category(const std::string& _name) : name(_name) {}

struct EntryPredicate
{
public:
	EntryPredicate(const std::string& _name) : name(_name) {}
	bool operator()(const Entry& e) const { return e.name == name; }
protected:
	std::string name;
};

void Category::addEntry(const std::string& entry, const std::string& value)
{
	DOMINIQS_ASSERT( entry != "" );
	EntryList::iterator itr = find_if(entries.begin(), entries.end(), EntryPredicate(entry));
	if ( itr == entries.end() ) entries.push_back(Entry(entry, value));
	else itr->value = value;
}

void Category::deleteEntry(const std::string& entry)
{
	DOMINIQS_ASSERT( entry != "" );
	entries.erase(remove_if(entries.begin(), entries.end(), EntryPredicate(entry)), entries.end());
}

std::string Category::getEntry(const std::string& entry, const std::string& defValue) const
{
	DOMINIQS_ASSERT( entry != "" );
	EntryList::const_iterator itr = find_if(entries.begin(), entries.end(), EntryPredicate(entry));
	if ( itr == entries.end() ) return defValue;
	else return itr->value;
}

Category::Iterator Category::begin() const
{
	return entries.begin();
}

Category::Iterator Category::end() const
{
	return entries.end();
}

std::ostream& operator<<(std::ostream& out, const Category& c)
{
	out << "[" << c.name << "]" << std::endl;
	Category::Iterator itr = c.begin();
	Category::Iterator end = c.end();
	while( itr != end )
	{
		out << itr->name << " = " << itr->value << std::endl;
		++itr;
	}
	return out;
}

bool IniFile::load(const std::string& filename)
{
	IniParserContext ctx(this);

	// GRAMMAR

	// helper rules
	rule_t char_ident_start = alpha_p | ch_p('_') ;
	rule_t char_ident_middle = alnum_p | ch_p('_') ;
	rule_t ident = char_ident_start >> * char_ident_middle ;
	rule_t char_start_comment = ch_p('#') | ch_p(';') | str_p("//");
	rule_t blanks_p = * blank_p;
	rule_t value_p = * ( alnum_p | blank_p | punct_p );

	// empty lines and comments
	rule_t l_comment = blanks_p >> char_start_comment >> * print_p >> eol_p;
	rule_t l_empty = blanks_p >> eol_p;
	rule_t c_comment_rule = confix_p("/*", *anychar_p, "*/");
	rule_t b_comment =
	                blanks_p >>
	                c_comment_rule >>
	                blanks_p >>
	                eol_p;

	// category
	rule_t l_category =
	                blanks_p >>
	                ch_p('[') >>
	                blanks_p >>
	                ident [ boost::bind(&IniParserContext::addCategory, &ctx, _1, _2) ] >>
	                blanks_p >>
	                ch_p(']') >>
	                blanks_p >>
	                eol_p;

	// entry
	rule_t l_entry =
	                blanks_p >>
	                ident [ boost::bind(&IniParserContext::addEntry, &ctx, _1, _2) ] >>
	                blanks_p >>
	                ch_p('=') >>
	                blanks_p >>
	                value_p [ boost::bind(&IniParserContext::addValue, &ctx, _1, _2) ] >>
	                blanks_p >>
	                eol_p;

	// whole file
	rule_t lines = l_comment | b_comment | l_category | l_entry | l_empty;
	rule_t ini_file =  lexeme_d [ * lines ] ;

	// Create a file iterator for this file
	iterator_t first(filename.c_str());
	if (!first) return false;
	// Create an EOF iterator
	iterator_t last = first.make_end();
	int errcode = parse(first, last, ini_file).full;
	return errcode;
}

void IniFile::save(const std::string& filename) const
{
	std::ofstream out(filename.c_str());
	save(out);
}

void IniFile::save(std::ostream& out) const
{
	CategoryList::const_iterator itr = categories.begin();
	CategoryList::const_iterator end = categories.end();
	while( itr != end )
	{
		out << **itr << std::endl;
		++itr;
	}
}

struct CategoryPredicate
{
public:
	CategoryPredicate(const std::string& _name) : name(_name) {}
	bool operator()(CategoryPtr c) const { return c && (c->name == name); }
protected:
	std::string name;
};

CategoryPtr IniFile::addCategory(const std::string& name)
{
	DOMINIQS_ASSERT( name != "" );
	CategoryPtr cat = getCategory(name);
	if (!cat)
	{
		CategoryPtr toAdd(new Category(name));
		categories.push_back(toAdd);
	}
	return getCategory(name);
}

void IniFile::addCategory(CategoryPtr cat)
{
	DOMINIQS_ASSERT( cat );
	deleteCategory(cat->name);
	categories.push_back(cat);
}

CategoryPtr IniFile::getCategory(const std::string& name) const
{
	CategoryList::const_iterator itr = find_if(categories.begin(), categories.end(), CategoryPredicate(name));
	if ( itr != categories.end() ) return (*itr);
	return CategoryPtr();
}

void IniFile::deleteCategory(const std::string& name)
{
	categories.erase(remove_if(categories.begin(), categories.end(), CategoryPredicate(name)), categories.end());
}

std::string IniFile::get(const std::string& cat, const std::string& entry, const std::string& defValue) const
{
	CategoryPtr c = getCategory(cat);
	if (c) return c->getEntry(entry, defValue);
	else return defValue;
}

void IniFile::set(const std::string& cat, const std::string& entry, const std::string& value)
{
	CategoryPtr c = getCategory(cat);
	if (!c)
	{
		c = CategoryPtr(new Category(cat));
		addCategory(c);
		c = getCategory(cat);
	}
	c->addEntry(entry, value);
}

IniFile& gConfig2()
{
	static IniFile theFile;
	return theFile;
}

} // namespace dominiqs
