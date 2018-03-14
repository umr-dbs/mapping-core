#include "util/parameters.h"
#include "exceptions.h"
#include <string>
#include <algorithm>

/*
 * Parameters
 */
bool Parameters::hasParam(const std::string& key) const {
    return find(key) != end();
}

const std::string &Parameters::get(const std::string &name) const {
    auto it = find(name);
    if (it == end())
        throw ArgumentException(concat("No parameter found for key ", name));
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
        throw ArgumentException(concat("No parameter found for key ", name));
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
        throw ArgumentException(concat("No parameter found for key ", name));
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
        throw ArgumentException(concat("No parameter found for key ", name));
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
            result.insert(std::make_pair(key.substr(prefix.length()), it.second));
        }
    }
    return result;
}

std::vector<std::string> Parameters::getAll(const std::string &name) const {
    std::vector<std::string> vec;

    auto iteratorPair = this->equal_range(name);
    for(auto it = iteratorPair.first; it != iteratorPair.second; it++){
        vec.push_back(it->second);
    }

    return vec;
}

std::vector<int> Parameters::getAllInt(const std::string &name) const {
    std::vector<int> vec;

    auto iteratorPair = this->equal_range(name);
    for(auto it = iteratorPair.first; it != iteratorPair.second; it++){
        vec.push_back(parseInt(it->second));
    }

    return vec;
}

std::vector<long> Parameters::getAllLong(const std::string &name) const {
    std::vector<long> vec;

    auto iteratorPair = this->equal_range(name);
    for(auto it = iteratorPair.first; it != iteratorPair.second; it++){
        vec.push_back(parseLong(it->second));
    }

    return vec;
}

std::vector<bool> Parameters::getAllBool(const std::string &name) const {
    std::vector<bool> vec;

    auto iteratorPair = this->equal_range(name);
    for(auto it = iteratorPair.first; it != iteratorPair.second; it++){
        vec.push_back(parseBool(it->second));
    }

    return vec;
}

const std::string &Parameters::getLast(const std::string &name) const {
    auto range = equal_range(name);
    if(range.first == range.second)
        throw ArgumentException(concat("No parameter found for key ", name));

    auto it = range.second;
    it--;
    return it->second;
}

const std::string &Parameters::getLast(const std::string &name, const std::string &defaultValue) const {
    auto range = equal_range(name);
    if(range.first == range.second)
        return defaultValue;

    auto it = range.second;
    it--;
    return it->second;
}

int Parameters::getLastInt(const std::string &name) const {
    auto range = equal_range(name);
    if(range.first == range.second)
        throw ArgumentException(concat("No parameter found for key ", name));

    auto it = range.second;
    it--;
    return parseInt(it->second);
}

int Parameters::getLastInt(const std::string &name, int defaultValue) const {
    auto range = equal_range(name);
    if(range.first == range.second)
        return defaultValue;

    auto it = range.second;
    it--;
    return parseInt(it->second);
}

long Parameters::getLastLong(const std::string &name) const {
    auto range = equal_range(name);
    if(range.first == range.second)
        throw ArgumentException(concat("No parameter found for key ", name));

    auto it = range.second;
    it--;
    return parseLong(it->second);
}

long Parameters::getLastLong(const std::string &name, long defaultValue) const {
    auto range = equal_range(name);
    if(range.first == range.second)
        return defaultValue;

    auto it = range.second;
    it--;
    return parseLong(it->second);
}

bool Parameters::getLastBool(const std::string &name) const {
    auto range = equal_range(name);
    if(range.first == range.second)
        throw ArgumentException(concat("No parameter found for key ", name));

    auto it = range.second;
    it--;
    return parseBool(it->second);
}

bool Parameters::getLastBool(const std::string &name, bool defaultValue) const {
    auto range = equal_range(name);
    if(range.first == range.second)
        return defaultValue;

    auto it = range.second;
    it--;
    return parseBool(it->second);
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

