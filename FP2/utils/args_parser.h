/**
 * @file args_parser.h
 * @brief Command line parsing utilities Header
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2007-2012
 */

#ifndef ARGS_PARSER_H
#define ARGS_PARSER_H

#include <string>
#include <vector>
#include <map>

namespace dominiqs {

/**
 * Command-line argument parser:
 * Accepts:
 * --config (-c): config XML file (multiple times allowed)
 * -C: config override
 * --output (-o): output (logging?) file
 * All unmatched words are put into input vector
 */

class ArgsParser
{
public:
	ArgsParser();
	void reset();
	// parsing
	bool parse(int argc, char const* argv[]);
	bool parse(const std::vector<std::string>& tokens);
	bool parse(const std::string& data);
	// data
	std::vector<std::string> config;
	std::vector<std::string> overrides;
	std::string output;
	std::vector<std::string> input;
};

class XMLConfig;

/**
 * Reads all config INI files (from left to right)
 * specified in the command line
 * and merge into a single one. Then it overrides some
 * config values according to the command line
 * It optionally uses a string shortcut map while
 * doing command line overrides
 */

typedef std::map<std::string, std::string> ShortcutMap;

void mergeConfig(const ArgsParser& args, XMLConfig& config, const ShortcutMap& shortcuts = ShortcutMap());

} // namespace dominiqs

#endif /* ARGS_PARSER_H */
