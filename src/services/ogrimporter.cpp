
#include "util/ogr_source_datasets.h"
#include "util/uploader_util.h"
#include "util/ogr_source_util.h"
#include "services/httpservice.h"

/**
 * Import Service for OGR Source. It will import a whole upload directory of the user. At the moment it
 * does not delete the upload after a successful import.
 *
 * If the dataset does not exists, it will be created. Else the new layer can be appended, but that has to be
 * activated explicitly with the append_dataset parameter. One importer call imports only one layer, so multiple
 * layers in a dataset require multiple importer calls.
 *
 * WFS links can also be imported: The WFS server link has to be provided in the main_file parameter, beginning with "WFS:".
 * The dataset name has to be empty or not provided, because no dataset is used.
 *
 * Parameters:
 *  - sessiontoken
 *  - upload_name: name of the upload that is to be imported
 *  - main_file: name of the main file (to be opened by OGR Source)
 *  - dataset_name: name for the dataset that is created
 *  - layer_name: name of the layer that the below parameters are provided for
 *  - append_dataset: bool, if an existing dataset will be appended. Defaults to false.
 *
 *  These dataset information also have to be provided as parameters:
 * - time: the type of the time column(s): "none", "start", "start+end", "start+duration"
 * - duration: the duration of the time validity for all features in the file [if time == "start"]
 * - time1_format: "custom", "seconds", "dmyhm", "iso" [if time != "none"]
 * - time1_custom_format: format string for parsing time1 attribute. [if time1_format == "custom"]
 * - time2_format: "custom", "seconds", "dmyhm", "iso" [if time == "start+end" || "start+duration"]
 * - time2_custom_format: format string for parsing time2 attribute. [if time2_format == "custom"]
 * - x: the name of the column containing the x coordinate (or the wkt string) [if CSV file]
 * - y: the name of the column containing the y coordinate [if CSV file with y column]
 * - time1: the name of the first time column [if time != "none"]
 * - time2: the name of the second time column [if time == "start+end" || "start+duration"]
 * - on_error: specify the type of error handling: "skip", "abort", "keep"
 * - citation
 * - license
 * - uri
 * - default: wkt definition of the default point/line/polygon as a string [optional]
 * - description: describing the layer [optional]
 * - force_ogr_time_filter: bool [optional]
 * - geometry_type: needed when OGR does not know the geometry type, esp. for csv files [optional]
 */

namespace bf = boost::filesystem;

class OGRImporter : public HTTPService {
public:
    using HTTPService::HTTPService;
    virtual ~OGRImporter() = default;
    virtual void run();
private:
    Json::Value createLayerJson(const bf::path &target_dir, const bool isWFS);
    void testFileValidity(const std::string &user_id, const std::string &upload_name, const std::string &main_file, const std::string &layer_name, const bool isWFS);
};

REGISTER_HTTP_SERVICE(OGRImporter, "IMPORTER_OGR");

void OGRImporter::run() {

    //check user session and permissions
    auto session = UserDB::loadSession(params.get("sessiontoken"));

    if(session->isExpired()){
        throw ImporterException("Not a valid user session.");
    }

    auto user = session->getUser();
    if(!user.hasPermission("upload") || !user.hasPermission("import_ogr")) {
        throw ImporterException("User does not have permission.");
    }

    const std::string user_id       = user.getUserIDString();
    const std::string ogr_dir       = Configuration::get<std::string>("ogrsource.files.path");
    const std::string upload_name   = params.get("upload_name", "");
    const std::string main_file     = params.get("main_file");
    const std::string dataset_name  = params.get("dataset_name");
    const std::string layer_name    = params.get("layer_name");
    const bool append_dataset       = params.getBool("append_dataset", false);

    const bool isWFS = main_file.find("WFS:") == 0;
    if(isWFS && !upload_name.empty()){
        throw ImporterException("WFS link detected in main_file (starts with 'WFS:'). In that case the "
                                "upload_name is expected to be empty, because it is not needed.");
    }

    bf::path import_target_dir(ogr_dir);
    import_target_dir /= params.get("dataset_name");
    bf::path dataset_json_path(ogr_dir);
    dataset_json_path /= dataset_name + ".json";

    if(!isWFS){
        //check if upload and main file exist
        const bool upload_exists = UploaderUtil::exists(user_id, upload_name)
                                   && UploaderUtil::uploadHasFile(user_id, upload_name, main_file);
        if(!upload_exists){
            throw ImporterException("Requested upload or main file does not exist.");
        }
    }

    //check if dataset with same name already exists
    const bool dataset_exists = bf::exists(dataset_json_path);
    if(dataset_exists && !append_dataset){
        throw ImporterException("OGRSource dataset with same name already exists. Appending was not requested.");
    }
    if(append_dataset && !dataset_exists){
        throw ImporterException("Dataset should be appended, but it does not exist already.");
    }

    //if dataset_json already exists load it and add the new layer to it. Else create new dataset json.
    Json::Value dataset_json(Json::ValueType::nullValue);
    if(dataset_exists){
        dataset_json = OGRSourceDatasets::getDatasetDescription(dataset_name);
    } else {
        dataset_json = Json::Value(Json::ValueType::objectValue);
    }
    if(!dataset_json.isMember("layers"))
        dataset_json["layers"] = Json::Value(Json::ValueType::objectValue);

    if(dataset_exists && dataset_json["layers"].isMember(layer_name)) {
        throw ImporterException("Layer can not be added to OGR Dataset because a layer with same name already exists: " + layer_name);
    }

    // test if the dataset & layer can be opened with OGR
    testFileValidity(user_id, upload_name, main_file, layer_name, isWFS);

    //create layer json and add it to dataset_json
    auto layer_json = createLayerJson(import_target_dir, isWFS);
    dataset_json["layers"][layer_name] = layer_json;

    if(!isWFS){
        //move uploaded files to OGR location, if error occurs delete the copied files
        std::vector<std::string> copied_files;
        copied_files.reserve(8); //avoid resizing for most cases
        try {
            UploaderUtil::moveUpload(user_id, upload_name, import_target_dir, copied_files);
        } catch(std::exception &e){
            //could not move upload, delete already copied files and directory.
            for(auto &filename : copied_files){
                bf::path file_path = import_target_dir;
                file_path /= filename;
                bf::remove(file_path);
            }
            response.sendFailureJSON("Could not copy files");
            return;
        }
    }

    //write dataset json to file, after moving the files, because that could go wrong.
    Json::StyledWriter writer;
    std::ofstream file;
    file.open(dataset_json_path.string());
    file << writer.write(dataset_json);
    file.close();

    user.addPermission("data.ogr_source." + dataset_name);
    response.sendSuccessJSON();
}

/**
 * write the needed parameters into the dataset json. Tests some conditions and throws exceptions when they are not met.
 */
Json::Value OGRImporter::createLayerJson(const bf::path &target_dir, const bool isWFS) {
    Json::Value layer_json(Json::ValueType::objectValue);

    if(ErrorHandlingConverter.is_value(params.get("on_error")))
        layer_json["on_error"] = params.get("on_error");
    else
        throw ImporterException("Invalid 'on_error' parameter.");

    if(TimeSpecificationConverter.is_value(params.get("time")))
        layer_json["time"] = params.get("time");
    else
        throw ImporterException("Invalid time parameter.");

    if(params.hasParam("duration"))
        layer_json["duration"] = params.get("duration");
    else if(params.get("time") == "start")
        throw ImporterException("For TimeSpecification 'start' a duration has to be provided.");

    if(params.hasParam("time1_format")){
        Json::Value time1(Json::ValueType::objectValue);
        std::string format = params.get("time1_format");
        time1["format"] = format;
        if(format == "custom"){
            time1["custom_format"] = params.get("time1_custom_format");
        }
        layer_json["time1_format"] = time1;
    }
    if(params.hasParam("time2_format")){
        Json::Value time2(Json::ValueType::objectValue);
        std::string format = params.get("time2_format");
        time2["format"] = format;
        if(format == "custom"){
            time2["custom_format"] = params.get("time2_custom_format");
        }
        layer_json["time2_format"] = time2;
    }

    Json::Value columns(Json::ValueType::objectValue);

    // if it is a csv file, it is already tested in testFileValidity (in openGDALDataset) that these
    // parameters are working or not provided and standard parameters work.
    if(params.hasParam("x"))
        columns["x"] = params.get("x");
    if(params.hasParam("y"))
        columns["y"] = params.get("y");

    if(params.hasParam("time1"))
        columns["time1"] = params.get("time1");
    else if(layer_json["time"].asString() != "none"){
        std::string msg("Selected time specification '");
        msg += layer_json["time"].asString();
        msg += "' requires a time1 attribute.";
        throw ImporterException(msg);
    }

    if(params.hasParam("time2"))
        columns["time2"] = params.get("time2");
    else if(layer_json["time"].asString() == "start+duration" || layer_json["time"].asString() == "start+end"){
        std::string msg("Selected time specification '");
        msg += layer_json["time"].asString();
        msg += "' requires a time2 attribute.";
        throw ImporterException(msg);
    }

    layer_json["columns"] = columns;

    if(params.hasParam("default"))
        layer_json["default"] = params.get("default");

    if(params.hasParam("description"))
        layer_json["description"] = params.get("description");

    if(params.hasParam("force_ogr_time_filter"))
        layer_json["force_ogr_time_filter"] = params.get("force_ogr_time_filter");

    if(params.hasParam("geometry_type"))
        layer_json["geometry_type"] = params.get("geometry_type");

    Json::Value provenance(Json::ValueType::objectValue);
    provenance["citation"] = params.get("citation");
    provenance["license"]  = params.get("license");
    provenance["uri"]      = params.get("uri");
    layer_json["provenance"] = provenance;

    if(isWFS){
        layer_json["filename"] = params.get("main_file");
    } else {
        bf::path file_path = target_dir;
        file_path /= params.get("main_file");
        layer_json["filename"] = file_path.string();
    }

    return layer_json;
}

/**
 * Tests if the given main file of the upload can be opened with OGR and has valid data
 */
void OGRImporter::testFileValidity(const std::string &user_id, const std::string &upload_name, const std::string &main_file, const std::string &layer_name, const bool isWFS) {
    bf::path upload_path;
    if(isWFS) {
        upload_path = main_file;
    } else {
        upload_path = UploaderUtil::getUploadPath(user_id, upload_name);
        upload_path /= main_file;
    }

    // opening the datasets needs filename, and x,y info if it is a csv file, as Json.
    Json::Value param_json(Json::ValueType::objectValue);
    param_json["filename"] = upload_path.string();
    Json::Value column_json(Json::ValueType::objectValue);
    if(params.hasParam("x")) {
        column_json["x"] = params.get("x");
    }
    if(params.hasParam("y")) {
        column_json["y"] = params.get("y");
    }
    param_json["column"] = column_json;

    auto dataset = OGRSourceUtil::openGDALDataset(param_json);
    if(dataset == nullptr){
        throw ImporterException("Can not load open main file with OGR");
    }

    OGRLayer *layer = dataset->GetLayerByName(layer_name.c_str());
    if(layer == nullptr){
        GDALClose(dataset);
        throw ImporterException("The imported layer does not exist");
    }

    GDALClose(dataset);
}
