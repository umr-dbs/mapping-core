
#ifndef MAPPING_CORE_GDAL_SOURCE_DATASETS_H
#define MAPPING_CORE_GDAL_SOURCE_DATASETS_H

#include "userdb/userdb.h"

#include <vector>
#include <json/value.h>


class GDALSourceDataSets {
public:
    /**
     * get the available data sets in the gdal source data sets directory
     */
    static std::vector<std::string> getDataSetNames();


    /**
     * get the data set description of the given data set
     * @param dataSetName
     */
    static Json::Value getDataSetDescription(const std::string &dataSetName);

};


#endif //MAPPING_CORE_GDAL_SOURCE_DATASETS_H
