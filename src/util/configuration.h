#ifndef UTIL_CONFIGURATION_H_
#define UTIL_CONFIGURATION_H_

#include <string>
#include <map>
#include <vector>
#include "cpptoml.h"
#include "util/exceptions.h"

/**
 * Wrapping cpptoml::table for smaller consistent access to the main configuration table via Configuration::get<T>(...)
 * and the subtables with:
 * 		ConfigurationTable subtable = Configuration::getSubTable(...);
 * 		subtable.get<T>(...)
 *
 *
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

        /// \tparam T use int64_t instead of int
        template <class T>
        std::vector<T> getVector(const std::string &name) {
            auto array_option = table->get_qualified_array_of<T>(name);
            if(array_option)
                return *array_option;
            else
                throw ArgumentException("Configuration: \'" + name + "\' not found as array.");
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

//specialization to allow usage with type int, because cpptoml only allows int64_t as a type. will create a copy of original vector.
template<>
std::vector<int> ConfigurationTable::getVector<int>(const std::string &name);


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

        template <class T>
        static std::vector<T> getVector(const std::string &name){
            return table.getVector<T>(name);
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
