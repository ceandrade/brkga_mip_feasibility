/**
 * @file logger.h
 * @brief Logging classes/utilities
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2011-2012
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include <boost/format.hpp>

#include "serialization.h"
#include "asserter.h"
#include "str_utils.h"


namespace dominiqs {

/**
 * Debug flag: define DOMINIQS_DEBUG to enable debug logging
 */

#ifdef DOMINIQS_DEBUG
#define DOMINIQS_DEBUG_LOG( action ) do { action; } while(false)
#else
#define DOMINIQS_DEBUG_LOG( unused ) do {} while(false)
#endif // DOMINIQS_DEBUG

/**
 * Main Logger Class
 * Used to log both binary and text data
 */

class Logger
{
public:
	// Constructors & destructors
	Logger() : doWrite(true), doEcho(false) {}
	~Logger();
	// file operations
	void open(const std::string& name, const std::string& path = "");
	void close();
	void flush();
	bool isOpen() const { return xml.is_open(); }
	// echo to console
	void setConsoleEcho(bool enable) { doEcho = enable; }
	void setFileWrite(bool enable) { doWrite = enable; }
	// logger main interface
	void startSection(const std::string& name);
	template<class T> void startSection(const std::string& name, const std::string& attrName, const T& attrValue)
	{
		DOMINIQS_ASSERT( name != "" );
		DOMINIQS_ASSERT( attrName != "" );
		if (doWrite)
		{
			if (!xml) throw std::runtime_error("Logging stream not opened yet!");
			xml << indent << "<" << name << " " << attrName << "=\"" << attrValue << "\">" << std::endl;
		}
		if (doEcho) std::clog << indent << boost::format(">%1%[%2%=%3%]") % name % attrName % attrValue << std::endl;
		openSections.push_back(name);
		indent.push_back('\t');
	}
	void endSection();
	template<class T> void logItem(const std::string& key, const T& value)
	{
		DOMINIQS_ASSERT( key != "" );
		if (doWrite)
		{
			if (!xml) throw std::runtime_error("Logging stream not opened yet!");
			xml << indent << "<" << key << ">" << value << "</" << key << ">" << std::endl;
		}
		if (doEcho) std::clog << indent << boost::format("%1% = %2%") % key % value << std::endl;
	}
	template<class T> void logBinaryItem(const std::string& key, const std::string& type, const T& value)
	{
		DOMINIQS_ASSERT( key != "" );
		if (doWrite)
		{
			if (!xml) throw std::runtime_error("Logging stream not opened yet!");
			xml << indent << "<" << key << " type=\"";
			xmlEscape(type.begin(), type.end(), xml);
			xml << "\">";
			s.serialize(value, xml);
			xml << "</" << key << ">" << std::endl;
		}
	}
	void logMsg(const std::string& msg);
protected:
	std::ofstream xml;
	Serializer s;
	std::string indent;
	std::vector<std::string> openSections;
	bool doWrite;
	bool doEcho;
};

typedef boost::shared_ptr<Logger> LoggerPtr;

/**
 * Global Logger
 */

Logger& gLog();

/**
 * Automatic Section Closer (RAII principle) for a given stopwatch
 */

class AutoSection
{
public:
	AutoSection(LoggerPtr l, const std::string& name) : theLogger(l), pending(true)
	{
		DOMINIQS_ASSERT( theLogger );
		theLogger->startSection(name);
	}
	template<class T>
	AutoSection(LoggerPtr l, const std::string& name, const std::string& attrName, const T& attrValue)
		: theLogger(l), pending(true)
	{
		DOMINIQS_ASSERT( theLogger );
		theLogger->startSection(name, attrName, attrValue);
	}
	~AutoSection();
	void close()
	{
		if (pending)
		{
			theLogger->endSection();
			pending = false;
		}
	}
private:
	LoggerPtr theLogger;
	bool pending;
};

/**
 * Automatic Section Closer (RAII principle) for the global stopewatch
 */

class GlobalAutoSection
{
public:
	GlobalAutoSection(const std::string& name)
		: pending(true)
	{
		gLog().startSection(name);
	}
	template<class T>
	GlobalAutoSection(const std::string& name, const std::string& attrName, const T& attrValue)
		: pending(true)
	{
		gLog().startSection(name, attrName, attrValue);
	}
	~GlobalAutoSection();
	void close()
	{
		if (pending)
		{
			gLog().endSection();
			pending = false;
		}
	}
private:
	bool pending;
};

} // namespace dominiqs

#endif /* LOGGER_H */
