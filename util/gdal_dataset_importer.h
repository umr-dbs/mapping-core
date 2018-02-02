#ifndef UTIL_GDAL_DATASET_IMPORTER_H_
#define UTIL_GDAL_DATASET_IMPORTER_H_

#include <string>
#include <json/json.h>
#include "datatypes/raster/raster_priv.h"
#include "util/gdal.h"

/*
 * Class for creating a Json dataset file used for the GDALSource Operator.
 * A dataset contains information about multiple raster files with different temporal validity.
 * It is defined by a start time and an interval value and unit. Foreach interval step a single 
 * raster file exists in the datasets directory.
 * The raster files are in the same directory and all have the "same" name containing a different time string
 * of the same format.
 * The GDALSource Operator will snap the queried time down to the time of an existing file.
 * time_unit: Year, Month, Day, Hour, Minute, Second
 * The GDALDataset Json files are all stored in the same directory defined in the mapping.conf with gdalsource.datasetpath.
 */
class GDALDatasetImporter {
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

};

#endif