#ifndef UTIL_ENUMCONVERTER_H
#define UTIL_ENUMCONVERTER_H

#include "util/exceptions.h"
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <json/json.h>


/*
 * Templates for mapping enums to strings and back.
 *
 * Mostly used for parameter parsing. Don't use it for enums with lots of values.
 *
 * For an example how to use this, see operators/source/csv_source.cpp
 */
template<typename T>
class EnumConverter {
	public:
        explicit EnumConverter(const std::vector<std::pair<T, std::string>> &map) : map(map), default_value("") {};

		EnumConverter(const std::vector<std::pair<T, std::string>> &map, std::string default_value) : map(map),
			default_value(std::move(default_value)) {};

		const std::string &to_string(T t) const {
			for (auto &tuple : map) {
				if (tuple.first == t)
					return tuple.second;
			}
			throw ArgumentException("No string found for enum value");
		}

		T from_string(const std::string &s) const {
			for (auto &tuple : map) {
				if (tuple.second == s)
					return tuple.first;
			}
			throw ArgumentException(concat("No enum value found for identifier \"", s, "\""));
		}

		T from_json(const Json::Value &root, const std::string &name) const {
			auto str = root.get(name, default_value).asString();
			return from_string(str);
		}

		bool is_value(const std::string &s){
			for (auto &tuple : map) {
				if (tuple.second == s)
					return true;
			}
			return false;
		}
	private:
		const std::vector< std::pair<T, std::string>> &map;
		const std::string default_value;
};

#endif
