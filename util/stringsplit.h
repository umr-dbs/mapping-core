#ifndef UTIL_STRINGSPLIT_H
#define UTIL_STRINGSPLIT_H


#include <string>
#include <sstream>
#include <vector>
#include <iterator>

template<typename T>
void split(const std::string &s, char delim, T result) {
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> result;
    split(s, delim, std::back_inserter(result));
    return result;
}

#endif
