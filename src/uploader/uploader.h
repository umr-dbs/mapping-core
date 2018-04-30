
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
 * Convention for the multipart post request that is expected by the UploadService:
 *
 *
 */
class UploadService {
public:
    UploadService(std::streambuf *in, std::streambuf *out, std::streambuf *err);
    void run();
private:
    void runIntern();
    Parameters parseParameters(Poco::Net::MultipartReader &mr);
    void uploadFile(Poco::Net::MultipartReader &mr, std::string &base_path, Parameters &params, bool isFirstFile);

    std::istream input;
    std::ostream error;
    HTTPService::HTTPResponseStream response;
};

#endif //MAPPING_CORE_UPLOADER_H
