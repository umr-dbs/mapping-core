#include "util/exceptions.h"
#include "util/configuration.h"

#include <iostream>
#include <unistd.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>

extern char **environ;


/*
 * Parameters
 */
bool Parameters::hasParam(const std::string& key) const {
    return find(key) != end();
}

const std::string &Parameters::get(const std::string &name) const {
    auto it = find(name);
    if (it == end())
        throw ArgumentException(concat("No configuration found for key ", name));
    return it->second;
}

const std::string &Parameters::get(const std::string &name, const std::string &defaultValue) const {
    auto it = find(name);
    if (it == end())
        return defaultValue;
    return it->second;
}

int Parameters::getInt(const std::string &name) const {
    auto it = find(name);
    if (it == end())
        throw ArgumentException(concat("No configuration found for key ", name));
    return parseInt(it->second);
}

int Parameters::getInt(const std::string &name, const int defaultValue) const {
    auto it = find(name);
    if (it == end())
        return defaultValue;
    return parseInt(it->second);
}

long Parameters::getLong(const std::string &name) const {
    auto it = find(name);
    if (it == end())
        throw ArgumentException(concat("No configuration found for key ", name));
    return parseLong(it->second);
}

long Parameters::getLong(const std::string &name, const long defaultValue) const {
    auto it = find(name);
    if (it == end())
        return defaultValue;
    return parseLong(it->second);
}

bool Parameters::getBool(const std::string &name) const {
    auto it = find(name);
    if (it == end())
        throw ArgumentException(concat("No configuration found for key ", name));
    return parseBool(it->second);
}

bool Parameters::getBool(const std::string &name, const bool defaultValue) const {
    auto it = find(name);
    if (it == end())
        return defaultValue;
    return parseBool(it->second);
}

Parameters Parameters::getPrefixedParameters(const std::string &prefix) {
    Parameters result;
    for (auto &it : *this) {
        auto &key = it.first;
        if (key.length() > prefix.length() && key.substr(0, prefix.length()) == prefix) {
            result[ key.substr(prefix.length()) ] = it.second;
        }
    }
    return result;
}


int Parameters::parseInt(const std::string &str) {
    return std::stoi(str); // stoi throws if no conversion could be performed
}

long Parameters::parseLong(const std::string &str) {
    return std::stol(str); // stol throws if no conversion could be performed
}


bool Parameters::parseBool(const std::string &str) {
    if (str == "1")
        return true;
    if (str == "0")
        return false;
    std::string strtl;
    strtl.resize( str.length() );
    std::transform(str.cbegin(), str.cend(), strtl.begin(), ::tolower);

    if (strtl == "true" || strtl == "yes")
        return true;
    if (strtl == "false" || strtl == "no")
        return false;

    throw ArgumentException(concat("'", str, "' is not a boolean value (try setting 0/1, yes/no or true/false)"));
}




/*
 * Configuration
 */
ConfigurationTable Configuration::table(cpptoml::make_table());

template <class T>
T ConfigurationTable::get(const std::string& name){
    auto item = table->get_qualified_as<T>(name);
    if(item)
        return *item;
    else
        throw ArgumentException("Configuration: \'" + name + "\' not found in subtable.");
}


/*
 * Insert another TOML table into the main Configuration table
 */
void Configuration::insertIntoMainTable(std::shared_ptr<cpptoml::table> other) {
    //it is iterator for a map<string, shared_ptr<base>>
    for(auto it = other->begin(); it != other->end(); ++it){
        table.getTomlTable()->insert(it->first, it->second);
    }
}

/*
 * Parsing a string as it were a TOML file.
 */
void Configuration::loadFromString(const std::string &content) {
    std::istringstream stream(content);
    cpptoml::parser p(stream);
    auto config = p.parse();
    insertIntoMainTable(config);
}

void Configuration::loadFromFile(const std::string &filename) {
    try {
        //Load toml file and insert every key-value-pair into table.
        auto config = cpptoml::parse_file(filename);
        insertIntoMainTable(config);

    } catch(cpptoml::parse_exception& e){
        std::cerr << "Configuration file load exception: " << e.what() << std::endl;
    }
}

void Configuration::loadFromEnvironment() {
    if (environ == nullptr)
        return;
    std::string configuration_file;
    std::string relevant_vars;

    for(int i=0;environ[i] != nullptr;i++) {
        auto line = environ[i];
        if (strncmp(line, "MAPPING_", 8) == 0 || strncmp(line, "mapping_", 8) == 0) {
            std::string linestr(&line[8]);
            if(linestr.length() >= 14 && (linestr.compare(0,14, "CONFIGURATION=") == 0 || linestr.compare(0,14, "configuration=") == 0)) {
                configuration_file = linestr.substr(14);
            }
            else {
                relevant_vars += linestr + "\n";
            }
        }
    }

    // The file must be loaded before we parse the variables, to guarantee a repeatable priority when multiple settings overlap
    if (configuration_file != "")
        loadFromFile(configuration_file);

    loadFromString(relevant_vars);
}

static const char* getHomeDirectory() {
	// Note that $HOME is not set for the cgi-bin executed by apache
	auto homedir = getenv("HOME");
	if (homedir)
		return homedir;

	auto pw = getpwuid(getuid());
	return pw->pw_dir;

	return nullptr;
}

static bool loaded_from_default_paths = false;

void Configuration::loadFromDefaultPaths() {
	if (loaded_from_default_paths)
		return;
	loaded_from_default_paths = true;

	loadFromFile("/etc/mapping.conf");

	auto homedir = getHomeDirectory();
	if (homedir && strlen(homedir) > 0) {
		std::string path = "";
		path += homedir;
		path += "/mapping.conf";
        loadFromFile(path.c_str());
	}

    loadFromFile("./mapping.conf");
	loadFromEnvironment();
}
