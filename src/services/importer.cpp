
#include "ogcservice.h"
#include "util/exceptions.h"
#include <boost/filesystem.hpp>


/**
 * Needed Parameters:
 *      - uploadDirectory
 *
 * Phases:
 *      - "upload": upload the file(s) as POST request.
 *      -> response telling which importers are possible
 *      - "import": upload import information to file, needed:
 *         - filename
 *         - importer used
 *         - parameters for the importer
 */
class ImportService : public OGCService {
public:
    using OGCService::OGCService;
    ~ImportService() override = default;
    virtual void run();
};

REGISTER_HTTP_SERVICE(ImportService, "importer");

void ImportService::run() {

    auto session = UserDB::loadSession(params.get("sessiontoken"));
    auto user = session->getUser();
    const std::string &username = user.getUsername();

    auto phase = params.get("phase");

    if(phase == "upload"){

        int fileCount   = params.getInt("fileCount");

        std::string uploadDir = Configuration::get<std::string>("uploadDirectory");

        for(int i = 0; i < fileCount; i++){
            auto isBinary   = params.getBool("isBinary" + i);
            auto filename   = params.get("filename" + i);
            auto file       = params.get("file" + i);

            std::string filepath = uploadDir + "/" + filename;

            std::ofstream::openmode openmode = std::ofstream::out;
            if(isBinary)
                openmode = std::ofstream::binary;

            std::ofstream fileStream(filepath, openmode);
            fileStream << file;
            fileStream.close();

        }

    } else if(phase == "import"){



    } else {
        throw ImporterException("Importer: invalid requested phase " + phase + ".");
    }








}
