
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include "ogr_source_datasets.h"
#include "configuration.h"
#include "ogr_source_util.h"

std::vector<std::string> OGRSourceDatasets::getDatasetNames(){
    namespace bf = boost::filesystem;
    bf::path path = Configuration::get<std::string>("ogrsource.files.path");

    if(!bf::is_directory(path))
        throw ArgumentException("ogrsource.files.path in configuration is not valid directory");


    std::vector<std::string> filenames;
    const std::string suffix(".json");
    const size_t suffix_length = suffix.length();

    for(auto it = bf::directory_iterator(path); it != bf::directory_iterator{}; ++it){
        auto file = (*it).path();
        if(!bf::is_regular_file(file))
            continue;

        //check if file ends with the suffix
        const std::string file_ext = bf::extension(file);

        if (suffix == file_ext) {
            const std::string filename = file.filename().string();
            const std::string dataSetName = filename.substr(0, filename.length() - suffix_length);
            filenames.push_back(dataSetName);
        }
    }
    return filenames;
}rasterdb

Json::Value OGRSourceDatasets::getDatasetListing(const std::string &dataset_name){
    Json::Value desc = getDatasetDescription(dataset_name);
    Json::Value root;
    Json::Value columns = desc["columns"];

    bool isCsv = OGRSourceUtil::hasSuffix(desc["filename"].asString(), ".csv") || OGRSourceUtil::hasSuffix(desc["filename"].asString(), ".tsv");

    Json::Value layer_array(Json::ValueType::arrayValue);

    GDALAllRegister();
    GDALDataset *dataset = OGRSourceUtil::openGDALDataset(desc);

    if(dataset == nullptr){
        throw OperatorException("OGR Source Datasets: Can not load dataset");
    }

    const int layer_count = dataset->GetLayerCount();

    for(int i = 0; i < layer_count; i++){
        OGRLayer *layer = dataset->GetLayer(i);

        std::string layer_name(layer->GetName());
        Json::Value layer_object(Json::ValueType::objectValue);
        layer_object["name"] = layer_name;

        // if layer does not have a geometry type (eg CSV files), a field "geometry_type" in the dataset json should be defined
        auto geom_type = layer->GetGeomType();
        if(geom_type != OGRwkbGeometryType::wkbUnknown){
            layer_object["geometry_type"] = OGRGeometryTypeToName(geom_type);
        }
        else{
            layer_object["geometry_type"] = desc.get("geometry_type", "Unknown");
        }

        // To get the TITLE from meta data, check if metadata is not empty,
        // than access the title that could still be Null.
        layer_object["title"] = "";
        char ** metadata = layer->GetMetadata(nullptr);
        if( CSLCount(metadata) > 0 ){
            const char* title = layer->GetMetadataItem("TITLE");
            if(title != nullptr)
                layer_object["title"] = title;
        }

        Json::Value textual(Json::ValueType::arrayValue);
        Json::Value numeric(Json::ValueType::arrayValue);

        OGRFeatureDefn *attributeDefn = layer->GetLayerDefn();

        for(int j = 0; j < attributeDefn->GetFieldCount(); j++){
            OGRFieldDefn *field = attributeDefn->GetFieldDefn(j);
            std::string field_name  = field->GetNameRef();

            //make sure not to add geometry columns if it is csv file
            if(isCsv)
            {
                if(field_name == columns["x"].asString() ||
                   columns.isMember("y") && field_name == columns["y"].asString())
                {
                    continue;
                }
            }

            //don't add time columns
            if(columns.isMember("time1") && field_name == columns["time1"].asString() ||
               columns.isMember("time2") && field_name == columns["time2"].asString())
            {
                continue;
            }

            OGRFieldType type = field->GetType();

            //TODO: what to do with different types for the attributes, eg time? Right now it's added as textual
            if(type == OFTInteger || type == OFTInteger64 || type == OFTReal)
                numeric.append(field_name);
            else
                textual.append(field_name);
        }

        layer_object["textual"] = textual;
        layer_object["numeric"] = numeric;
        layer_array.append(layer_object);
    }

    root["layer"] = layer_array;

    GDALClose(dataset);

    return root;
}

Json::Value OGRSourceDatasets::getDatasetDescription(const std::string &name){
    std::string directory = Configuration::get<std::string>("ogrsource.files.path");
    boost::filesystem::path file_path(directory);
    file_path /= name + ".json";

    std::ifstream file(file_path.string());
    if (!file.is_open()) {
        throw ArgumentException("OGR Source Datasets: File with given name not found -> " + name);
    }

    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;
    bool success = reader.parse(file, root);
    if (!success) {
        throw ArgumentException("OGR Source Datasets: invalid json file");
    }

    return root;
}
