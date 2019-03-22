
#include "datatypes/plots/json.h"

#include <json/json.h>

JsonPlot::JsonPlot(Json::Value json) : json(json) {

}
JsonPlot::~JsonPlot() {

}

const std::string JsonPlot::toJSON() const {
	Json::Value root(Json::ValueType::objectValue);

	root["type"] = "json";
	root["data"] = json;

	Json::FastWriter writer;
	return writer.write( root );
}
