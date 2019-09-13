/**
 * @file it_display.cpp
 * @brief Iteration Display Class
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * Copyright 2009 Domenico Salvagnin
 */

#include "it_display.h"

namespace dominiqs {

bool IterationDisplay::addColumn(const std::string& name, int p, int w, bool v, const std::string& d)
{
	for (auto& c: columns)
	{
		if (c.second->name == name) return false;
	}
	columns[p] = new Column(name, p, w, v, d);
	return true;
}

void IterationDisplay::removeColumn(const std::string& name)
{
	ColumnMap::iterator itr = columns.begin();
	while(itr != columns.end())
	{
		ColumnMap::iterator here = itr++;
		if((*here).second->name == name)
		{
			Column* c = (*here).second;
			columns.erase(here);
			delete c;
		}
	}
}

void IterationDisplay::setVisible(const std::string& name, bool visible)
{
	for (auto& c: columns)
	{
		if (c.second->name == name) c.second->visible = visible;
	}
}

void IterationDisplay::printHeader(std::ostream& out)
{
	if (columns.empty()) return;
	for (auto& c: columns)
	{
		if (c.second->visible)
		{
			out << std::setw(c.second->width) << c.second->name;
		}
	}
	out << std::endl;
}

void IterationDisplay::set(const std::string& name, FmtPtr data)
{
	current[name] = data;
}

void IterationDisplay::resetIteration()
{
	current.clear();
	marked = false;
}

void IterationDisplay::printIteration(std::ostream& out)
{
	if (columns.empty()) return;
	for (auto& c: columns)
	{
		if (c.second->visible)
		{
			if (current.find(c.second->name) != current.end()) current[c.second->name]->print(out, c.second->width);
			else out << std::setw(c.second->width) << std::string(" ");
		}
	}
	out << std::endl;
}

} // namespace dominiqs
