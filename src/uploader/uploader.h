
#ifndef MAPPING_CORE_UPLOADER_H
#define MAPPING_CORE_UPLOADER_H

#include "util/parameters.h"
#include <Poco/URI.h>
#include <Poco/Base64Decoder.h>
#include <Poco/Net/MultipartReader.h>
#include <Poco/Net/MessageHeader.h>
#include "services/httpservice.h"

using namespace Poco::Net;

/**
 * Service for uploading files to mapping. This just stores them in an upload directory.
 * The service expects a multipart post request.
 *
 * Convention for the multipart post request:
 *  - the first part contains parameters, encoded as URL parameter: param1=val1&param2=val2
 *  - then follow one or more file parts:
 *      + Content-Disposition is form-data and a filename parameter is provided.
 *      + the file is written in the parts body.
 *
 * parameters needed:
 *  - sessiontoken
 *  - upload_name: name for the upload.
 *  - append_upload: bool indicating if an upload with the same name already exists and should be appended by this upload.
 */
class UploadService {
public:
    UploadService(std::streambuf *in, std::streambuf *out, std::streambuf *err);
    void run();
private:
    void runIntern();
    Parameters parseParameters(Poco::Net::MultipartReader &mr);
    void uploadFile(Poco::Net::MultipartReader &mr, std::string &base_path, std::vector<std::string> &files_writen);

    std::istream input;
    std::ostream error;
    HTTPService::HTTPResponseStream response;
};

#endif //MAPPING_CORE_UPLOADER_H
