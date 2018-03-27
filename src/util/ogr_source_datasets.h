
#ifndef MAPPING_CORE_OGR_SOURCE_DATASETS_H
#define MAPPING_CORE_OGR_SOURCE_DATASETS_H

#include <json/json.h>

class OGRSourceDatasets {
public:
    static std::vector<std::string> getFileNames();
    static Json::Value getFileDescription(const std::string &name);
    static Json::Value getListing(const std::string &name);
};

#endif //MAPPING_CORE_OGR_SOURCE_DATASETS_H
