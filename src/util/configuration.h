#ifndef UTIL_CONFIGURATION_H_
#define UTIL_CONFIGURATION_H_

#include <string>
#include <map>
#include "cpptoml.h"
#include "util/exceptions.h"

class Parameters : public std::map<std::string, std::string> {
public:
	bool hasParam(const std::string& key) const;

	const std::string &get(const std::string &name) const;
	const std::string &get(const std::string &name, const std::string &defaultValue) const;
	int getInt(const std::string &name) const;
	int getInt(const std::string &name, int defaultValue) const;
	long getLong(const std::string &name) const;
	long getLong(const std::string &name, long defaultValue) const;
	bool getBool(const std::string &name) const;
	bool getBool(const std::string &name, bool defaultValue) const;

	/**
     * Returns all parameters with a given prefix, with the prefix stripped.
     * For example, if you have the configurations
     *  my.module.paramA = 50
     *  my.module.paramB = 20
     * then parameters.getPrefixedParameters("my.module.") will return a Parameters object with
     *  paramA = 50
     *  paramB = 20
     *
     * @param prefix the prefix of the interesting parameter names. Usually, this should end with a dot.
     */
	Parameters getPrefixedParameters(const std::string &prefix);

	// These do throw exceptions when the string cannot be parsed.
	static int parseInt(const std::string &str);
	static long parseLong(const std::string &str);
	static bool parseBool(const std::string &str);
};

/**
 * Wrapping cpptoml::table for smaller consistent access to the main configuration table via Configuration::get<T>(...)
 * and the subtables with:
 * 		ConfigurationTable subtable = Configuration::getSubTable(...);
 * 		subtable.get<T>(...)
 *
 * If more functionality is needed getTomlTable will give access to the underlying cpptoml::table.
 *
 */
class ConfigurationTable {
	public:
		ConfigurationTable(std::shared_ptr<cpptoml::table> table) : table(table) { }

	template <class T>
	T get(const std::string& name){
        auto item = table->get_qualified_as<T>(name);
        if(item)
            return *item;
        else
            throw ArgumentException("Configuration: \'" + name + "\' not found in subtable.");
    }

	template <class T>
	T get(const std::string& name, const T& alternative){
		return table->get_qualified_as<T>(name).value_or(alternative);
	}

	ConfigurationTable getSubTable(const std::string& name){
		return ConfigurationTable(table->get_table_qualified(name));
	}

	bool contains(const std::string& key){
		return table->contains(key);
	}

	std::shared_ptr<cpptoml::table> getTomlTable(){
		return table;
	}

	private:
		std::shared_ptr<cpptoml::table> table;

};

/**
 * Class for loading the configuration of the application.
 * It loads key, value parameters from the following location in the given order
 * 1. /etc/mapping.conf
 * 2. working directory mapping.conf
 * 3. environment variables starting with MAPPING_ and mapping_
 *
 */

class Configuration {
	public:
        static void loadFromDefaultPaths();
        static void loadFromString(const std::string &content);
        static void loadFromFile(const std::string &filename);
    private:
        static ConfigurationTable table;
        static void loadFromEnvironment();
        static void insertIntoMainTable(std::shared_ptr<cpptoml::table> other);
	public:
		template <class T>
        static T get(const std::string& name){
            return table.get<T>(name);
		}
        template <class T>
        static T get(const std::string& name, T alternative){
            return table.get<T>(name, alternative);
        }

		static ConfigurationTable getSubTable(const std::string& name){
			return table.getSubTable(name);
		}

		static bool contains(const std::string& key){
			return table.contains(key);
		}

		static std::shared_ptr<cpptoml::table> getTomlTable(){
			return table.getTomlTable();
		}
};

#endif
