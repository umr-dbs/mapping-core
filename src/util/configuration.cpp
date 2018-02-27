#include "util/exceptions.h"
#include "util/configuration.h"

#include <iostream>
#include <unistd.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>

extern char **environ;

/*
 * Helpers for Configuration. Only needed for parsing the parameters from enviroment variables.
 */
static std::string stripWhitespace(const std::string &str) {
	const char *whitespace = " \t\r\n";
	auto start = str.find_first_not_of(whitespace);
	auto end = str.find_last_not_of(whitespace);
	if (start == std::string::npos || end == std::string::npos)
		return std::string("");

	return str.substr(start, end-start+1);
}

static std::string normalizeKey(const std::string &_key) {
	auto key = stripWhitespace(_key);
	for (size_t i=0;i<key.length();i++) {
		char c = key[i];
		if (c >= 'a' && c <= 'z')
			continue;
		if (c >= 'A' && c <= 'Z') {
			key[i] = c - 'A' + 'a';
			continue;
		}
		if (c == '_') {
			key[i] = '.';
			continue;
		}
		if ((c >= '0' && c <= '9') || (c == '.'))
			continue;

		// some other character -> the whole key is invalid
		std::cerr << "Error in configuration: invalid key name '" << _key << "'\n";
		return "";
	}
	return key;
}

///*
// * Configuration
// */
std::shared_ptr<cpptoml::table> Configuration::table;

//parseLine is needed for manually reading parameters from the enviroment variables.
void Configuration::parseLine(const std::string &_line) {
	auto line = stripWhitespace(_line);
	if (line.length() == 0 || line[0] == '#')
		return;

	auto pos = line.find_first_of('=');
	if (pos == std::string::npos) {
		std::cerr << "Error in configuration: not a key=value pair, line = '" << line << "'\n";
		return;
	}

	std::string key = normalizeKey(line.substr(0, pos));
	if (key == "")
		return;
	std::string value = stripWhitespace(line.substr(pos+1));

    table->insert(key, value);
    //TODO: Try parsing the values to the possible types. Else all environment parameters will be saved as strings.
}


void Configuration::load(const std::string &filename) {
    try {
        //Load toml file and insert every key-value-pair into table.
        auto config = cpptoml::parse_file(filename);

        //it is iterator for a map<string, shared_ptr<base>>
        for(auto it = config->begin(); it != config->end(); ++it){
            table->insert(it->first, it->second);
        }
    } catch(cpptoml::parse_exception& e){
        std::cerr << "Configuration file load exception: " << e.what() << std::endl;
    }
}


void Configuration::loadFromEnvironment() {
    if (environ == nullptr)
        return;
    std::string configuration_file;
    std::vector<std::string> relevant_vars;

    for(int i=0;environ[i] != nullptr;i++) {
        auto line = environ[i];
        if (strncmp(line, "MAPPING_", 8) == 0 || strncmp(line, "mapping_", 8) == 0) {
            std::string linestr(&line[8]);
            if (linestr.length() >= 14 && (strncmp(linestr.c_str(), "CONFIGURATION=", 14) || strncmp(linestr.c_str(), "configuration=", 14)))
                configuration_file = linestr.substr(14);
            else
                relevant_vars.push_back(linestr);
        }
    }

    // The file must be loaded before we parse the variables, to guarantee a repeatable priority when multiple settings overlap
    if (configuration_file != "")
        load(configuration_file);
    for (auto &linestr : relevant_vars){

        parseLine(linestr);
    }
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

	table = cpptoml::make_table();

	load("/etc/mapping.conf");

	auto homedir = getHomeDirectory();
	if (homedir && strlen(homedir) > 0) {
		std::string path = "";
		path += homedir;
		path += "/mapping.conf";
		load(path.c_str());
	}

	load("./mapping.conf");
	loadFromEnvironment();
}
