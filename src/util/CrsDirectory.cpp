#include "CrsDirectory.h"
#include "util/configuration.h"
#include "exceptions.h"

#include <fstream>
#include <json/json.h>

std::string CrsDirectory::getWKTForCrsId(const CrsId &crsId) {
    // TODO: load file only once
    const std::string filePath = Configuration::get("crsdirectory.location");

    //open file then read json object from it
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw ArgumentException("CrsDirectory: could not find directory file");
    }

    Json::Reader reader(Json::Features::strictMode());
    Json::Value directory;
    if (!reader.parse(file, directory)) {
        throw ArgumentException("CrsDirectory: directory file contains invalid json");
    }

    const std::string &crs = crsId.to_string();
    if(!directory.isMember(crs)) {
        return "";
    } else {
        return directory[crs].get("wkt", "").asString();
    }
}
