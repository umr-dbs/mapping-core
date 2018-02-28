#include "util/exceptions.h"
#include "util/configuration.h"

#include <iostream>
#include <unistd.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>

extern char **environ;

///*
// * Configuration
// */
std::shared_ptr<cpptoml::table> Configuration::table;

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
        load(configuration_file);

    //let cpptoml parse the parameters as if they were a toml file.
    std::istringstream stream(relevant_vars);
    cpptoml::parser p(stream);
    auto config = p.parse();
    for(auto it = config->begin(); it != config->end(); ++it){
        table->insert(it->first, it->second);
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
