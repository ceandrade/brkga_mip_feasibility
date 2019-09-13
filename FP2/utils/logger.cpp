/**
 * @file logger.cpp
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2007-2008
 */

#include <stdexcept>
#include <iostream>

#include "logger.h"
#include "path.h"

namespace dominiqs {

Logger::~Logger()
{
	try
	{
		close();
	}
	catch(std::exception& e)
	{
		std::clog << "Exception closing logger: " << e.what() << std::endl;
	}
}


void Logger::open(const std::string& name, const std::string& path)
{
	close();
	Path filename(path);
	filename.mkdir();
	filename /= name;
	xml.open(filename.getPath().c_str(), std::ios::out);
	xml << "<?xml version=\"1.0\"?>" << std::endl;
	xml << "<xmlog>" << std::endl;
}

void Logger::close()
{
	DOMINIQS_ASSERT( openSections.empty() );
	xml << "</xmlog>" << std::endl;
	xml.flush();
	xml.close();
	indent.clear();
}

void Logger::flush()
{
	xml.flush();
}

void Logger::startSection(const std::string& name)
{
	DOMINIQS_ASSERT( name != "" );
	if (doWrite)
	{
		if (!xml) throw std::runtime_error("Logging stream not opened yet!");
		xml << indent << "<" << name << ">" << std::endl;
	}
	if (doEcho) std::clog << indent << ">" << name << std::endl;
	openSections.push_back(name);
	indent.push_back('\t');
}

void Logger::endSection()
{
	DOMINIQS_ASSERT( !openSections.empty() );
	if (doWrite && !xml) throw std::runtime_error("Logging stream not opened yet!");
	indent.resize(indent.size() - 1);
	std::string name = openSections.back();
	xml << indent << "</" << name << ">" << std::endl;
	openSections.pop_back();
}

void Logger::logMsg(const std::string& msg)
{
	DOMINIQS_ASSERT( msg != "" );
	if (doWrite)
	{
		if (!xml) throw std::runtime_error("Logging stream not opened yet!");
		xml << indent << "<msg>";
		xmlEscape(msg.begin(), msg.end(), xml);
		xml << "</msg>" << std::endl;
	}
	if (doEcho) std::clog << indent << msg << std::endl;
}

Logger& gLog()
{
	static Logger theLogger;
	return theLogger;
}

AutoSection::~AutoSection()
{
	try
	{
		close();
	}
	catch(std::exception& e)
	{
		std::clog << "Exception in ~AutoSection: " << e.what() << std::endl;
	}
}

GlobalAutoSection::~GlobalAutoSection()
{
	try
	{
		close();
	}
	catch(std::exception& e)
	{
		std::clog << "Exception in ~AutoSection: " << e.what() << std::endl;
	}
}

} // namespace dominiqs
