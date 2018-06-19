
#include <util/uploader_util.h>
#include <util/ogr_source_util.h>
#include "services/httpservice.h"

/**
 * Import Service for OGR Source. It will import a whole upload directory of the user. At the moment it
 * does not delete the upload after a successful import.
 *
 * Parameters:
 *  - sessiontoken
 *  - upload_name: name of the upload that is to be imported
 *  - main_file: name of the main file (to be opened by OGR Source)
 *  - dataset_name: name for the dataset that is created
 *
 *  These dataset information also have to be provided as parameters:
 * - time: the type of the time column(s). "none", "start", "start+end", "start+duration" allows
 * - duration: the duration of the time validity for all features in the file [if time == "start"]
 * - time1_format: "custom", "seconds", "dmyhm", "iso" [if time != "none"]
 * - time1_custom_format: format string for parsing time1 attribute. [if time1_format == "custom"]
 * - time2_format: "custom", "seconds", "dmyhm", "iso" [if time == "start+end" || "start+duration"]
 * - time2_custom_format: format string for parsing time2 attribute. [if time2_format == "custom"]
 * - x: the name of the column containing the x coordinate (or the wkt string) [if CSV file]
 * - y: the name of the column containing the y coordinate [if CSV file with y column]
 * - time1: the name of the first time column [if time != "none"]
 * - time2: the name of the second time column [if time == "start+end" || "start+duration"]
 * - default: wkt defintion of the default point/line/polygon as a string [optional]
 * - on_error: specify the type of error handling: "skip", "abort", "keep"
 * - citation
 * - license
 * - uri
 *
 */
class OGRImporter : public HTTPService {
public:
    using HTTPService::HTTPService;
    virtual ~OGRImporter() = default;
    virtual void run();
private:
    Json::Value createJson(const boost::filesystem::path &target_dir);
    bool testFileValidity(const std::string &user_id, const std::string &upload_name, const std::string &main_file);
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
    const std::string user_id = user.getUserIDString();

    const std::string ogr_dir = Configuration::get<std::string>("ogrsource.files.path");
    boost::filesystem::path target_dir(ogr_dir);
    target_dir /= params.get("dataset_name");

    const std::string upload_name  = params.get("upload_name");
    const std::string main_file    = params.get("main_file");
    const std::string dataset_name = params.get("dataset_name");

    //check if upload and main file exist
    bool exists = UploaderUtil::exists(user_id, upload_name);
    exists &= UploaderUtil::uploadHasFile(user_id, upload_name, main_file);

    if(!exists){
        throw ImporterException("Requested upload or main file does not exist.");
    }

    //check if dataset with same name already exists
    boost::filesystem::path dataset_path(ogr_dir);
    dataset_path /= (dataset_name + ".json");
    if(boost::filesystem::exists(dataset_path))
        throw ImporterException("OGRSource dataset with same name already exists.");
    
    // test if the dataset can be opened with OGR
    bool valid = testFileValidity(user_id, upload_name, main_file);
    if(!valid)
        throw ImporterException("Uploaded files can not be opened with OGRSource.");

    //create dataset json
    auto dataset_json = createJson(target_dir);

    //moves uploaded files to OGR location.
    try {
        UploaderUtil::moveUpload(user_id, upload_name, target_dir);
    } catch(std::exception &e){
        //could not move upload, delete already copied files and directory.
        boost::filesystem::remove_all(target_dir);
        response.sendFailureJSON("Could not copy files");
        return;
    }

    //write dataset json to file, after moving the files, because that could go wrong.
    Json::StyledWriter writer;
    std::ofstream file;
    file.open(ogr_dir + dataset_name + ".json");
    file << writer.write(dataset_json);
    file.close();

    user.addPermission("data.ogr_source." + dataset_name);

    //send reponse
    response.sendSuccessJSON();
}

/**
 * Tests if the given main file of the upload can be opened with OGR and has valid data
 */
bool OGRImporter::testFileValidity(const std::string &user_id, const std::string &upload_name, const std::string &main_file) {
    auto upload_path = UploaderUtil::getUploadPath(user_id, upload_name);
    upload_path /= main_file;

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

    int layer_count = dataset->GetLayerCount();
    if(layer_count == 0){
        throw ImporterException("File does not have Layer.");
    }

    for (int i = 0; i < layer_count; ++i) {
        OGRLayer *layer = dataset->GetLayer(i);
        if(layer == nullptr){
            throw ImporterException("A layer in the file is null.");
        }
    }

    GDALClose(dataset);
    return true;
}

/**
 * write the needed parameters into the dataset json. Tests some conditions and throws exceptions when they are not met.
 */
Json::Value OGRImporter::createJson(const boost::filesystem::path &target_dir) {
    Json::Value dataset_json(Json::ValueType::objectValue);

    if(ErrorHandlingConverter.is_value(params.get("on_error")))
        dataset_json["on_error"] = params.get("on_error");
    else
        throw ImporterException("Invalid 'on_error' parameter.");

    if(TimeSpecificationConverter.is_value(params.get("time")))
        dataset_json["time"] = params.get("time");
    else
        throw ImporterException("Invalid time parameter.");

    if(params.hasParam("duration"))
        dataset_json["duration"] = params.get("duration");
    else if(params.get("time") == "start")
        throw ImporterException("For TimeSpecification 'start' a duration has to be provided.");

    if(params.hasParam("time1_format")){
        Json::Value time1(Json::ValueType::objectValue);
        std::string format = params.get("time1_format");
        time1["format"] = format;
        if(format == "custom"){
            time1["custom_format"] = params.get("time1_custom_format");
        }
        dataset_json["time1_format"] = time1;
    }
    if(params.hasParam("time2_format")){
        Json::Value time2(Json::ValueType::objectValue);
        std::string format = params.get("time2_format");
        time2["format"] = format;
        if(format == "custom"){
            time2["custom_format"] = params.get("time2_custom_format");
        }
        dataset_json["time2_format"] = time2;
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
    else if(dataset_json["time"].asString() != "none"){
        std::string msg("Selected time specification '");
        msg += dataset_json["time"].asString();
        msg += "' requires a time1 attribute.";
        throw ImporterException(msg);
    }

    if(params.hasParam("time2"))
        columns["time2"] = params.get("time2");
    else if(dataset_json["time"].asString() == "start+duration" || dataset_json["time"].asString() == "start+end"){
        std::string msg("Selected time specification '");
        msg += dataset_json["time"].asString();
        msg += "' requires a time2 attribute.";
        throw ImporterException(msg);
    }

    dataset_json["columns"] = columns;

    if(params.hasParam("default"))
        dataset_json["default"] = params.get("default");

    Json::Value provenance(Json::ValueType::objectValue);
    provenance["citation"] = params.get("citation");
    provenance["license"]  = params.get("license");
    provenance["uri"]      = params.get("uri");
    dataset_json["provenance"] = provenance;

    boost::filesystem::path file_path = target_dir;
    file_path /= params.get("main_file");
    dataset_json["filename"] = file_path.string();

    return dataset_json;
}
