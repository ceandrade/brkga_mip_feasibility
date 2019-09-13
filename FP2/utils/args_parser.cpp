/**
 * @file args_parser.cpp
 * @brief Command line parsing utilities
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2007-2012
 */
#include <boost/tokenizer.hpp>

#include "args_parser.h"
#include "xmlconfig.h"
#include "asserter.h"

namespace dominiqs {

ArgsParser::ArgsParser() {}

void ArgsParser::reset()
{
	input.clear();
	output = "";
	config.clear();
	overrides.clear();
}

bool ArgsParser::parse(const std::string& data)
{
	std::vector<std::string> tokens;
	typedef boost::tokenizer< boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep(" ");
	tokenizer tok(data, sep);
	for (auto token: tok) tokens.push_back(token);
	return parse(tokens);
}

bool ArgsParser::parse(int argc, char const* argv[])
{
	std::vector<std::string> tokens;
	for (int i = 1 ; i < argc; i++) tokens.push_back(std::string(argv[i]));
	return parse(tokens);
}

enum ParserStatus {DEFAULT, CONFIG, OVERRIDE, OUTPUT};

bool ArgsParser::parse(const std::vector<std::string>& tokens)
{
	std::ostringstream out;
	ParserStatus status = DEFAULT;
	for (const std::string& tok: tokens)
	{
		if (status == CONFIG)
		{
			config.push_back(tok);
			status = DEFAULT;
		}
		else if (status == OVERRIDE)
		{
			overrides.push_back(tok);
			status = DEFAULT;
		}
		else if (status == OUTPUT)
		{
			output = tok;
			status = DEFAULT;
		}
		else
		{
			// status == DEFAULT
			if (tok == "-c" || tok == "--config") status = CONFIG;
			else if (tok == "-C") status = OVERRIDE;
			else if (tok == "-o" || tok == "--output") status = OUTPUT;
			else input.push_back(tok);
		}
	}
	return true;
}

void mergeConfig(const ArgsParser& args, XMLConfig& config, const ShortcutMap& shortcuts)
{
	/* load config files */
	for (const std::string& conf: args.config) config.load(conf);
	/* overrides */
	for (const std::string& over: args.overrides)
	{
		size_t eqIdx = over.find_first_of('=');
		DOMINIQS_ASSERT( eqIdx != std::string::npos );
		std::string address(over.begin(), over.begin() + eqIdx);
		std::string value(over.begin() + eqIdx + 1, over.end());
		size_t dotIdx = over.find_first_of('.');
		DOMINIQS_ASSERT( dotIdx != std::string::npos );
		std::string catName(address.begin(), address.begin() + dotIdx);
		std::string elemName(address.begin() + dotIdx + 1, address.end());
		ShortcutMap::const_iterator res = shortcuts.find(catName);
		if (res != shortcuts.end()) catName = res->second;
		config.set(catName, elemName, value);
	}
}

} // namespace dominiqs
