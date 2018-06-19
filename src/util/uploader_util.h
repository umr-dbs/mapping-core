
#ifndef MAPPING_CORE_UPLOADER_UTIL_H
#define MAPPING_CORE_UPLOADER_UTIL_H

#include <boost/filesystem.hpp>
#include "userdb/userdb.h"

/**
 * Functionality for the import services to interact with uploads.
 * The caller has to check if the user has permission for these interactions, name of permission for uploader is "upload"
 */
class UploaderUtil {
public:
    static void moveUpload(const std::string &user_id, const std::string &upload_name, boost::filesystem::path &target_dir);
    static bool exists(const std::string &user_id, const std::string &upload_name);
    static bool uploadHasFile(const std::string &user_id, const std::string &upload_name, const std::string &file_name);
    static boost::filesystem::path getUploadPath(const std::string &user_id, const std::string &upload_name);
};


#endif //MAPPING_CORE_UPLOADER_UTIL_H
