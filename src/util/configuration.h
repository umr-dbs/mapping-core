
#ifndef UTIL_CONFIGURATION_H_
#define UTIL_CONFIGURATION_H_

#include <string>
#include <map>
#include "cpptoml.h"


/**
 * Class for loading the configuration of the application.
 * It loads key, value parameters from the following location in the given order
 * 1. /etc/mapping.conf
 * 2. working directory mapping.conf
 * 3. environment variables starting with MAPPING_ and mapping_
 *
 * New usage:
 *  *Configuration::table->get_qualified_as<int>("someTable.someParameter")
 *  *Configuration::table->get_as<int>("someValue")
 *
 *   the first example is a value from a nested table someTable, to get the
 *   value directly you have to use get_qualified_as<T>.
 *   sub-tables can be directly accessed via Configuration->table->get_table("someTable").
 *   Also possible: get_table_qualified.
 *
 */

class Configuration {
	public:
        static std::shared_ptr<cpptoml::table> table;
        static void loadFromDefaultPaths();
	private:
        static void load(const std::string &filename);
        static void loadFromEnvironment();
		static void parseLine(const std::string &line);
};

#endif
