/**
 * \file path.cpp
 * \brief Path management Source
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2011
 */

#include <string.h> //< for getcwd
#ifdef WIN32
#include <direct.h>
#else
#include <sys/stat.h> //< for mkdir
#endif
#include <errno.h>
#include <stdexcept>
#include <list>
#include <algorithm>
#include <functional>
#include <sstream>

#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/erase.hpp>

#include "path.h"

namespace dominiqs {

std::string getCwd()
{
	static const int DEFAULT_BUFFER_SIZE = 512;
	size_t size = DEFAULT_BUFFER_SIZE;
	while (true)
	{
		std::string s;
		char* buffer = (char*)malloc(size);
		if (getcwd(buffer, size) == buffer)
		{
			s = std::string(buffer);
			free(buffer);
			return s;
		}
		else
		{
			if (errno != ERANGE) return s;
			else size *= 2;
		}
		free(buffer);
	}
}

void Path::read(const char* path)
{
	data = std::string(path);
	cleanPath(data);
}

void Path::read(const std::string& path)
{
	data = path;
	cleanPath(data);
}

Path Path::operator/(const std::string& chunk)
{
	Path ret(data);
	ret /= chunk;
	return ret;
}

void Path::operator/=(const std::string& chunk)
{
	if (chunk.size())
	{
		if ((chunk[0] == '/') || (data.size() == 0)) data += chunk;
		else data += "/" + chunk;
		cleanPath(data);
	}
}

void Path::mkdir(bool recursive)
{
	if (data.empty()) return;
	if (!recursive)
	{
		int res = ::mkdir(data.c_str(), 0777);
		if (res && (errno != EEXIST)) throw std::runtime_error(strerror(errno));
		return;
	}
	else
	{
		std::string::size_type found;
		std::string::size_type index = 0;
		while(index != std::string::npos)
		{
			found = data.find_first_of('/', index);
			index = found < data.size() ? (found + 1) : std::string::npos;
			if (found != 0)
			{
				data[found] = '\0';
				int res = ::mkdir(data.c_str(), 0777);
				if (res && (errno != EEXIST)) throw std::runtime_error(strerror(errno));
				data[found] = '/';
			}
			if (found == std::string::npos) return;
		}
	}
}

bool Path::isEmpty() const
{
	return data.empty();
}

bool Path::isAbsolute() const
{
	return (!isEmpty() && data[0] == '/');
}

bool Path::isRelative() const
{
	return (!isEmpty() && data[0] != '/');
}

std::string Path::getAbsolutePath() const
{
	if (isAbsolute()) return data;
	// if path is relative, use current working dir to build absolute path
	std::string apath = getCwd() + "/" + data;
	cleanPath(apath);
	return apath;
}

std::string Path::getBasename() const
{
	if (isEmpty()) return std::string("");
	std::string::size_type start = data.find_last_of("/");
	if (start == std::string::npos) return data;
	return std::string(data.begin() + start + 1, data.end());
}

void Path::cleanPath(std::string& path) const
{
	if (isEmpty()) return;
	bool hasRoot = (path[0] == '/');
	// tokenize
	std::list<std::string> tokens;
	typedef boost::tokenizer< boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep("/");
	tokenizer tok(path, sep);
	for (tokenizer::iterator itr = tok.begin(); itr != tok.end(); ++itr) tokens.push_back(*itr);
	// sanity check
	if (hasRoot && tokens.size() && (*tokens.begin()) == std::string("..")) throw std::runtime_error(std::string("Bad path: ") + data);
	// remove single dots
	tokens.erase(remove_if(tokens.begin(), tokens.end(), std::bind2nd(std::equal_to<std::string>(), ".")), tokens.end());
	// remove double dots
	std::list<std::string>::iterator itr = std::find_if(tokens.begin(), tokens.end(), bind2nd(std::not_equal_to<std::string>(), ".."));
	std::list<std::string>::iterator end = tokens.end();
	while (itr != end)
	{
		if (*itr == std::string(".."))
		{
			std::list<std::string>::iterator tmp = itr++;
			--tmp;
			if (*tmp != std::string(".."))
			{
				tmp = tokens.erase(tmp);
				tmp = tokens.erase(tmp);
			}
		}
		else ++itr;
	}
	// rebuild string
	std::ostringstream out;
	if (hasRoot) out << "/";
	itr = tokens.begin();
	end = tokens.end();
	while (itr != end)
	{
		out << *itr++;
		if (itr != end) out << "/";
	}
	path = out.str();
}

std::string getProbName(const std::string& fileName, const std::vector<std::string>& exts)
{
	std::string probName = Path(fileName).getBasename();
	for (const std::string& ext: exts) boost::ierase_last(probName, ext);
	return probName;
}

std::string getProbName(const std::string& fileName)
{
	std::vector<std::string> exts;
	exts.push_back(".gz");
	exts.push_back(".bz2");
	exts.push_back(".mps");
	exts.push_back(".lp");
	return getProbName(fileName, exts);
}

} // namespace dominiqs
