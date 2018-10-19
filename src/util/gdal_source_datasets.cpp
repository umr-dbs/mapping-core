
#include <fstream>
#include <json/reader.h>
#include <boost/filesystem.hpp>

#include "gdal_source_datasets.h"
#include "configuration.h"
#include "exceptions.h"


const std::string suffix(".json");
const size_t suffix_length = suffix.length();

std::vector<std::string> GDALSourceDataSets::getDataSetNames() {
    namespace bf = boost::filesystem;
    const bf::path path(Configuration::get<std::string>("gdalsource.datasets.path"));

    if(!bf::is_directory(path))
        throw MustNotHappenException("GDAL_Service: Directory for gdal dataset files could not be found.");

    std::vector<std::string> dataSetNames;

    for(auto it = bf::directory_iterator(path); it != bf::directory_iterator{}; ++it){
        auto file = (*it).path();
        if(!bf::is_regular_file(file))
            continue;

        //check if file ends with the suffix
        const std::string file_ext = bf::extension(file);

        if(suffix == file_ext){
            const std::string filename = file.filename().string();
            const std::string dataSetName = filename.substr(0, filename.length() - suffix_length);
            dataSetNames.push_back(dataSetName);
        }

    }
    return dataSetNames;
}

Json::Value GDALSourceDataSets::getDataSetDescription(const std::string &dataSetName) {

    boost::filesystem::path file_path(Configuration::get<std::string>("gdalsource.datasets.path"));
    file_path /= (dataSetName + suffix);

    //open file then read json object from it
    std::ifstream file(file_path.string());
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
