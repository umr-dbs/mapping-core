#ifndef UTIL_GDAL_DATASET_IMPORTER_H_
#define UTIL_GDAL_DATASET_IMPORTER_H_

#include <string>
#include <json/json.h>
#include "datatypes/raster/raster_priv.h"
#include "util/gdal.h"

class DatasetImporter {
public:
	static void importDataset(std::string dataset_name, 
							  std::string dataset_filename_with_placeholder, 
							  std::string dataset_file_path, 
							  std::string time_format, 
							  std::string time_start, 
							  std::string time_unit, 
							  std::string interval_value, 
							  std::string citation, 
							  std::string license, 
							  std::string uri,
							  std::string measurement,
							  std::string unit,
							  std::string interpolation);


private:
	static const std::string placeholder;
	static GDALDataset* openGDALDataset(std::string file_name);
	static Json::Value readCoords(GDALDataset *dataset);
	static Json::Value readChannels(GDALDataset *dataset, std::string measurement, std::string unit, std::string interpolation);
	static std::string dataTypeToString(GDALDataType type);
	static std::string getEpsg(GDALDataset *dataset);

};

#endif