
#include <dirent.h>
#include "ogr_source_datasets.h"
#include "configuration.h"
#include "ogr_source_util.h"

std::vector<std::string> OGRSourceDatasets::getDatasetNames(){
    const std::string path = Configuration::get<std::string>("ogrsource.files.path");

    struct dirent *entry;
    DIR *dir = opendir(path.c_str());

    if (dir == nullptr) {
        throw MustNotHappenException("OGR Source Datasets: Directory for OGR files could not be found -> " + path);
    }

    std::vector<std::string> filenames;
    std::string suffix(".json");
    size_t suffix_length = suffix.length();

    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;

        //check if file ends with the suffix
        bool found = filename.find(suffix, filename.length() - suffix_length)  != std::string::npos;

        if (found) {
            std::string dataSetName = filename.substr(0, filename.length() - suffix_length);
            filenames.push_back(dataSetName);
        }
    }

    closedir(dir);

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
    std::string filename = directory + "/" + name + ".json";

    std::ifstream file(filename);
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
