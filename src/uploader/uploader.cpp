
#include "util/make_unique.h"
#include "util/configuration.h"
#include "util/log.h"
#include <iostream>
#include <fstream>
#include "services/httpservice.h"
#include "services/httpparsing.h"
#include "uploader/uploader.h"

UploadService::UploadService(std::streambuf *in, std::streambuf *out, std::streambuf *err)
        : input(in), error(err), response(out)
{

}

void UploadService::run(){
    try {
        //analog to HTTPService check these env variables
        std::string request_method = getenv_str("REQUEST_METHOD", false);
        std::string content_type = getenv_str("CONTENT_TYPE", true);

        if (request_method != "POST") {
            throw UploaderException("Uploader requires POST requests.");
        }
        if (content_type.find("multipart/form-data") != std::string::npos
            && content_type.find("multipart/mixed") != std::string::npos)
        {
            throw UploaderException("Uploader requires a multipart POST requests.");
        }

        runInternal();
    } catch (std::exception &e) {
        response.send500(concat("Invalid upload: ", e.what()));
    }
}

void UploadService::runInternal(){

    MultipartReader mr(input);

    if(!mr.hasNextPart()){
        throw UploaderException("Empty multipart request.");
    }

    // parse first part as url parameters
    Parameters params = parseParameters(mr);

    // check user session
    const std::string &sessiontoken = params.get("sessiontoken");
    auto session = UserDB::loadSession(sessiontoken);
    auto user = session->getUser();
    auto userID = user.getUserIDString();
    if(!user.hasPermission("upload")){
        throw OperatorException("User does not have permission to upload.");
    }
    const std::string upload_name = params.get("upload_name");
    // basic check if a slash is in the upload name. if the name is not a valid directory name in another way,
    // creating the directory will fail later on.
    if(upload_name.find('/') != std::string::npos)
        throw UploaderException("The upload name should not contain '/' as a character.");

    std::string upload_dir = Configuration::get<std::string>("uploader.directory");
    boost::filesystem::path total_path(upload_dir);
    total_path /= userID;
    total_path /= upload_name;

    if(!boost::filesystem::exists(total_path)){
        bool suc = boost::filesystem::create_directories(total_path);
        if(!suc)
            throw UploaderException("Could not create upload directory.");
    } else {
        //exists already so check if it is okay to append the upload?
        if(!params.getBool("append_upload", false))
            throw UploaderException("Upload with same name already exists");
    }
    std::vector<std::string> files_writen;

    //read the parts for the files
    while(mr.hasNextPart()){
        try {
            uploadFile(mr, total_path, files_writen);
        } catch(std::exception &e){
            // error on writing a file occured. Delete all writen files.
            for(auto &filename : files_writen){
                boost::filesystem::path to_delete = total_path;
                to_delete /= filename;
                boost::filesystem::remove(to_delete);
            }
            throw;
        }
    }

    Json::Value success_json(Json::ValueType::objectValue);
    success_json["upload_name"] = params.get("upload_name");
    response.sendSuccessJSON(success_json);
}

void UploadService::uploadFile(Poco::Net::MultipartReader &mr, const boost::filesystem::path &base_path, std::vector<std::string> &files_writen) {

    MessageHeader header;
    mr.nextPart(header);
    std::istream &partIn = mr.stream();

    if(!header.has("Content-Disposition"))
        throw UploaderException("Invalid multipart request for Uploader. Missing Content-Disposition.");

    std::string contentDispValue;
    NameValueCollection contentDispParams;
    header.splitParameters(header["Content-Disposition"], contentDispValue, contentDispParams);

    if(!contentDispParams.has("filename"))
        throw UploaderException("The Content-Disposition of a file part has to provide a 'filename' parameter.");

    if(contentDispValue == "form-data"){
        std::string filename = contentDispParams["filename"];
        //check if a file with that name already exists
        boost::filesystem::path path = base_path;
        path /= filename;
        std::ofstream file(path.string());
        files_writen.push_back(filename);

        if(header.has("Content-Transfer-Encoding")){
            std::string encoding = header.get("Content-Transfer-Encoding");
            if(encoding == "base64"){
                Poco::Base64Decoder decoder(mr.stream());
                file << decoder.rdbuf();
            } else {
                file.close();
                throw UploaderException(concat("Data encoding of file ", filename, " not supported: ", encoding));
            }
        } else {
            file << mr.stream().rdbuf();
        }
        file.close();
    } else {
        throw UploaderException(concat("Unexpected Content-Disposition for file \"", contentDispParams["filename"],"\". Expected is form-data, but got: ", contentDispValue));
    }
}

Parameters UploadService::parseParameters(Poco::Net::MultipartReader &mr){

    Parameters params;
    Poco::Net::MessageHeader header;
    mr.nextPart(header);
    std::istream &body = mr.stream();

    if(!header.has("Content-Type"))
        throw UploaderException("Invalid multipart request. Missing 'Content-Type' of parameters part. It is expected to be 'application/x-www-form-urlencoded'.");

    if(header.get("Content-Type") != "application/x-www-form-urlencoded"){
        throw UploaderException("Multipart request misses Parameters part as the first part of the request or the content-type is wrong.");
    }

    std::ostringstream strStream;
    strStream << body.rdbuf();

    Poco::URI uri;
    uri.setRawQuery(strStream.str()); //setRawQuery because the string is already encoded.
    Poco::URI::QueryParameters pa = uri.getQueryParameters();

    for(auto &e : pa){
        params.insert(e);
    }

    return params;
}
