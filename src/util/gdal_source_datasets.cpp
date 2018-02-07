
#include <dirent.h>
#include <fstream>
#include <json/reader.h>

#include "gdal_source_datasets.h"
#include "configuration.h"
#include "exceptions.h"


const std::string suffix(".json");
const size_t suffix_length = suffix.length();

std::vector<std::string> GDALSourceDataSets::getDataSetNames() {
    const std::string path = Configuration::get("gdalsource.datasets.path");

    struct dirent *entry;
    DIR *dir = opendir(path.c_str());

    if (dir == nullptr) {
        throw MustNotHappenException("GDAL_Service: Directory for gdal dataset files could not be found.");
    }

    std::vector<std::string> dataSetNames;

    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;

        //check if file ends with the suffix
        bool found = filename.find(suffix, filename.length() - suffix_length)  != std::string::npos;

        if (found) {
            std::string dataSetName = filename.substr(0, filename.length() - suffix_length);
            dataSetNames.push_back(dataSetName);
        }
    }

    closedir(dir);

    return dataSetNames;
}

Json::Value GDALSourceDataSets::getDataSetDescription(const std::string &dataSetName) {
    const std::string path = Configuration::get("gdalsource.datasets.path");

    std::string fileName = path + "/" + dataSetName + suffix;

    //open file then read json object from it
    std::ifstream file(fileName);
    if (!file.is_open()) {
        throw ArgumentException("GDAlSourceDataSets: Data set with given name not found");
    }

    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;
    if (!reader.parse(file, root)) {
        throw ArgumentException("GDALSourceDataSets: invalid json file");
    }

    return root;
}
