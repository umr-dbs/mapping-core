
#include <iostream>
#include "util/uploader_util.h"
#include "util/exceptions.h"
#include "util/configuration.h"

namespace bf = boost::filesystem;

void UploaderUtil::moveUpload(const std::string &user_id, const std::string &upload_name, bf::path &target_dir, std::vector<std::string> &copied_files)
{
    bf::path upload_path = getUploadPath(user_id, upload_name);

    if(!bf::exists(upload_path) || !bf::is_directory(upload_path))
        throw UploaderException(concat("Requested upload '", upload_name,"' does not exist"));

    if(!bf::exists(target_dir))
        bf::create_directory(target_dir);

    for(auto it = bf::directory_iterator(upload_path); it != bf::directory_iterator{}; ++it){
        std::string filename = it->path().filename().string();
        bf::path file_target(target_dir);
        file_target /= filename;
        if(!bf::exists(file_target)){
            bf::copy_file(it->path(), file_target, bf::copy_option::fail_if_exists);
            copied_files.emplace_back(filename);
        }
    }
}

bool UploaderUtil::exists(const std::string &user_id, const std::string &upload_name) {
    bf::path path = getUploadPath(user_id, upload_name);
    return bf::exists(path) & bf::is_directory(path);
}

bool UploaderUtil::uploadHasFile(const std::string &user_id, const std::string &upload_name, const std::string &file_name) {
    bf::path path = getUploadPath(user_id, upload_name);
    path /= file_name;
    return bf::exists(path) & bf::is_regular_file(path);
}

bf::path UploaderUtil::getUploadPath(const std::string &user_id, const std::string &upload_name) {
    bf::path upload_path(Configuration::get<std::string>("uploader.directory"));
    upload_path /= user_id;
    upload_path /= upload_name;
    return upload_path;
}
