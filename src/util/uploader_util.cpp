
#include <iostream>
#include "util/uploader_util.h"
#include "util/exceptions.h"
#include "util/configuration.h"

using namespace boost::filesystem;

void UploaderUtil::moveUpload(const std::string &username, const std::string &upload_name, boost::filesystem::path &target_dir)
{
    path upload_path = getUploadPath(username, upload_name);

    if(!boost::filesystem::exists(upload_path) || !is_directory(upload_path))
        throw UploaderException(concat("Requested upload '", upload_name,"' does not exist"));

    if(!boost::filesystem::exists(target_dir))
        create_directory(target_dir);

    for(auto it = directory_iterator(upload_path); it != directory_iterator{}; ++it){
        std::cout << it->path() << std::endl;

        path file_target(target_dir);
        file_target /= it->path().filename();

        std::cout << file_target << std::endl;

        copy_file(it->path(), file_target, copy_option::fail_if_exists);
    }
}

bool UploaderUtil::exists(const std::string &username, const std::string &upload_name) {
    path path = getUploadPath(username, upload_name);
    return boost::filesystem::exists(path) & is_directory(path);
}

bool UploaderUtil::uploadHasFile(const std::string &username, const std::string &upload_name, const std::string &file_name) {
    path path = getUploadPath(username, upload_name);
    return boost::filesystem::exists(path) & is_directory(path);
}

boost::filesystem::path UploaderUtil::getUploadPath(const std::string &username, const std::string &upload_name) {
    path upload_path(Configuration::get<std::string>("uploader.directory"));
    upload_path /= username;
    upload_path /= upload_name;
    return upload_path;
}
