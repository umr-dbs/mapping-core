
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
}

Json::Value OGRSourceDatasets::getDatasetListing(const std::string &dataset_name){
    Json::Value dataset_def = getDatasetDescription(dataset_name);

    GDALAllRegister();
    GDALDataset *dataset = OGRSourceUtil::openGDALDataset(dataset_def);

    if(dataset == nullptr){
        throw OperatorException("OGR Source Datasets: Can not load dataset");
    }

    const std::string filename = dataset_def["filename"].asString();
    const bool isCsv = OGRSourceUtil::hasSuffix(filename, ".csv") || OGRSourceUtil::hasSuffix(filename, ".tsv");

    Json::Value listing_array(Json::ValueType::arrayValue);
    Json::Value all_layers_def = dataset_def["layers"];
    Json::Value columns_dataset = dataset_def["columns"];
    Json::Value root;

    for(auto &curr_layer_name : all_layers_def.getMemberNames()){
        OGRLayer *layer = dataset->GetLayerByName(curr_layer_name.c_str());
        std::string layer_name(layer->GetName());
        Json::Value layer_def = all_layers_def[layer_name];
        Json::Value columns_layer = layer_def["columns"];

        Json::Value listing_json(Json::ValueType::objectValue);
        listing_json["name"]  = layer_name;

        // Get the spatial ref information from the dataset
        OGRSpatialReference *layer_srs_ref = layer->GetSpatialRef();
        if(layer_srs_ref == nullptr) {
            // TODO: handle  this case in a fancy way
	        listing_json["coords"] = layer_def["coords"];
        } else {
 	        std::string srs_auth_code = layer_srs_ref->GetAuthorityCode(nullptr);
            std::string srs_auth_name = layer_srs_ref->GetAuthorityName(nullptr);
            listing_json["coords"]["crs"] = srs_auth_name + ":" + srs_auth_code;
        }

        // if layer does not have a geometry type (eg CSV files), check "geometry_type" in the layer/dataset json
        auto geom_type = layer->GetGeomType();
        if(geom_type != OGRwkbGeometryType::wkbUnknown){
            listing_json["geometry_type"] = OGRGeometryTypeToName(geom_type);
        }
        else if(layer_def.isMember("geometry_type")){
            listing_json["geometry_type"] = layer_def["geometry_type"];
        } else {
            listing_json["geometry_type"] = dataset_def.get("geometry_type", "Unknown");
        }

        if(layer_def.isMember("description")){
            listing_json["title"] = layer_def["description"];
        } else if(dataset_def.isMember("description")){
            listing_json["title"] = dataset_def["description"];
        } else {
            // To get the TITLE from meta data, check if metadata is not empty,
            // than access the title that could still be Null.
            listing_json["title"] = "";
            char ** metadata = layer->GetMetadata(nullptr); //documentation does not mentions that this has to be freed.
            if( CSLCount(metadata) > 0 ){
                const char* title = layer->GetMetadataItem("TITLE");
                if(title != nullptr)
                    listing_json["title"] = title;
            }
        }

        Json::Value textual(Json::ValueType::arrayValue);
        Json::Value numeric(Json::ValueType::arrayValue);

        OGRFeatureDefn *attributeDefn = layer->GetLayerDefn();

        for(int j = 0; j < attributeDefn->GetFieldCount(); j++){
            OGRFieldDefn *field = attributeDefn->GetFieldDefn(j);
            std::string field_name  = field->GetNameRef();

            if(isCsv)
            {
                //skip geometry attributes of csv files
                if(field_name == getJsonParameter(columns_layer, columns_dataset, "x").asString() ||
                        hasJsonParameter(columns_layer, columns_dataset, "y") &&
                   field_name == getJsonParameter(columns_layer, columns_dataset, "y").asString())
                {
                    continue;
                }
            }

            //skip time attributes
            if(hasJsonParameter(columns_layer, columns_dataset, "time1") &&
               field_name == getJsonParameter(columns_layer, columns_dataset, "time1").asString() ||
                    hasJsonParameter(columns_layer, columns_dataset, "time2") &&
               field_name == getJsonParameter(columns_layer, columns_dataset, "time2").asString())
            {
                continue;
            }

            OGRFieldType type = field->GetType();

            if(type == OFTInteger || type == OFTInteger64 || type == OFTReal)
                numeric.append(field_name);
            else
                textual.append(field_name);
        }

        listing_json["textual"] = textual;
        listing_json["numeric"] = numeric;
        listing_array.append(listing_json);
    }

    root["layer"] = listing_array;

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
        throw ArgumentException("OGR Source Datasets: invalid json file: " + name);
    }

    return root;
}

bool OGRSourceDatasets::hasJsonParameter(Json::Value &layer, Json::Value &dataset, const std::string &key){
    return layer.isMember(key) || dataset.isMember(key);
}

Json::Value OGRSourceDatasets::getJsonParameterDefault(Json::Value &layer, Json::Value &dataset, const std::string &key,
                                                const Json::Value &def){
    if(layer.isMember(key))
        return layer[key];
    else if(dataset.isMember(key))
        return dataset[key];
    else
        return def;
}

Json::Value OGRSourceDatasets::getJsonParameter(Json::Value &layer, Json::Value &dataset, const std::string &key){
    if(layer.isMember(key))
        return layer[key];
    else
        return dataset[key];
}
