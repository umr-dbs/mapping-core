
#include "util/make_unique.h"
#include "util/configuration.h"
#include "util/log.h"
#include <iostream>
#include <fstream>
#include "services/httpservice.h"
#include "uploader/uploader.h"
#include <boost/filesystem.hpp>

UploadService::UploadService(std::streambuf *in, std::streambuf *out, std::streambuf *err)
        : input(in), error(err), response(out)
{

}

void UploadService::run(){
    try {
        runIntern();
    } catch (std::exception &e) {
        response.send500(concat("Invalid upload: ", e.what()));
    }
}

void UploadService::runIntern(){

    MultipartReader mr(input);

    if(!mr.hasNextPart()){
        throw UploaderException("UploadService: Empty multipart request.");
    }

    // parse first part as url parameters
    Parameters params = parseParameters(mr);

    // check user session
    //TODO: remove guest user usage
    auto session_created = UserDB::createSession("guest", "guest", 8*3600);
    const std::string &sessiontoken = params.hasParam("sessiontoken") ? params.get("sessiontoken") : session_created->getSessiontoken();
    auto session = UserDB::loadSession(sessiontoken);
    auto user = session->getUser();
    auto username = user.getUsername();

    //TODO: use boost filesystem
    auto upload_dir = Configuration::get<std::string>("uploader.directory");
    auto total_path = upload_dir + username + "/";


    bool isFirstFile = true;

    //read the parts for the files
    while(mr.hasNextPart()){
        try {
            uploadFile(mr, total_path, params, isFirstFile);
        } catch(std::exception &e){
            // if total_path does not end on 'username + "/"' delete that whole directory again
            // because upload could not be finished.
            auto orig_path = upload_dir + username + "/";
            if(total_path.length() > orig_path.length())
                boost::filesystem::remove_all(total_path);
            throw;
        }

        isFirstFile = false;
    }

    response.sendSuccessJSON();

}

void UploadService::uploadFile(Poco::Net::MultipartReader &mr, std::string &base_path, Parameters &params, bool isFirstFile) {

    MessageHeader header;
    mr.nextPart(header);
    std::istream &partIn = mr.stream();

    if(!header.has("Content-Disposition"))
        throw UploaderException("Invalid multipart request for Uploader. Missing Content-Disposition.");

    std::string contentDispValue;
    NameValueCollection contentDispParams;
    header.splitParameters(header["Content-Disposition"], contentDispValue, contentDispParams);

    if(contentDispValue == "form-data"){

        if(isFirstFile){
            base_path.append(params.get("name") + "/");
            boost::filesystem::path p(base_path);
            if(!boost::filesystem::exists(base_path)){
                bool suc = boost::filesystem::create_directories(base_path);
                if(!suc)
                    throw UploaderException("Uploader: Could not create upload directory.");
            }
        }

        std::string filename = contentDispParams["filename"];
        //check if a file with that name already exists
        std::string path = base_path + filename;

        std::ofstream file(path);

        if(header.has("Content-Transfer-Encoding")){
            std::string encoding = header.get("Content-Transfer-Encoding");
            if(encoding == "base64"){
                Poco::Base64Decoder decoder(mr.stream());
                file << decoder.rdbuf();
            } else {
                file.close();
                throw UploaderException("UploadService: Data encoding of multipart not supported: " + encoding);
            }
        } else {
            file << mr.stream().rdbuf();
        }
        file.close();
    } else {
        throw UploaderException("Unexpected Content-Disposition in multipart. Expected is form-data, but got: " + contentDispValue);
    }
}

Parameters UploadService::parseParameters(Poco::Net::MultipartReader &mr){

    Parameters params;

    Poco::Net::MessageHeader header;
    mr.nextPart(header);
    std::istream &body = mr.stream();

    if(header.get("Content-Type") != "application/x-www-form-urlencoded"){
        throw UploaderException("UploadService: Multipart request misses Parameters part as the first part of the request or the content-type is wrong.");
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
