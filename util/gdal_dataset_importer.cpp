#include "gdal_dataset_importer.h"
#include "gdal_timesnap.h"
#include "configuration.h"
#include "exceptions.h"
#include "timeparser.h"
#include <iostream> 
#include <fstream>

const std::string DatasetImporter::placeholder = "%%%TIME_STRING%%%";

GDALDataset* DatasetImporter::openGDALDataset(std::string file_name){

	GDAL::init();

	GDALDataset *dataset = (GDALDataset *) GDALOpen(file_name.c_str(), GA_ReadOnly);

	if (dataset == NULL)
		throw ImporterException(concat("GDAL Source: Could not open dataset ", file_name));	

	return dataset;
}


void DatasetImporter::importDataset(std::string dataset_name, 
									std::string dataset_filename_with_placeholder, 
									std::string dataset_file_path, 
									std::string time_format, 
									std::string time_start, 
									std::string time_unit, 
									std::string interval_value, 
									std::string citation, 
									std::string license, 
									std::string uri){
		
	size_t placeholderPos =	dataset_filename_with_placeholder.find(placeholder);

	std::string datasetJsonPath = Configuration::get("gdalsource.datasetpath");

	if(placeholderPos == std::string::npos){
		throw ImporterException("GDAL DatasetImporter: Date placeholder " + placeholder + " not found in dataset filename " + dataset_filename_with_placeholder);
	}



	std::cout << "Interval value: " << interval_value << std::endl;

	TimeUnit tu;
	int interval = std::stoi(interval_value);

	if(GDALTimesnap::string_to_TimeUnit.find(time_unit) == GDALTimesnap::string_to_TimeUnit.end()){		
		throw ImporterException("GDAL DatasetImporter: " + time_unit + " is not a valid time unit (Year, Month, Day, Hour, Minute or Second)");
	} else {
		tu = GDALTimesnap::createTimeUnit(time_unit);
	}

	//actually this would not be necessary anymore for the way the time is snapped
	if(tu != TimeUnit::Year && (GDALTimesnap::maxValueForTimeUnit(tu) % interval) != 0){
		throw ImporterException("GDAL DatasetImporter: max unit of time unit has to be multiple of interval value, eg for Month (12): 4 is okay, 5 not ");
	}

	//parse time_start with time_format to check if its valid, else parse throws an exception
	auto timeParser = TimeParser::createCustom(time_format); 
	timeParser->parse(time_start);

	//create json 
	Json::Value timeIntervalJson(Json::ValueType::objectValue);
	timeIntervalJson["unit"] 	= time_unit;
	timeIntervalJson["value"] 	= interval;

	Json::Value datasetJson(Json::ValueType::objectValue);
	datasetJson["dataset_name"] = dataset_name;
	datasetJson["path"] 		= dataset_file_path;
	datasetJson["file_name"]	= dataset_filename_with_placeholder;
	datasetJson["time_format"] 	= time_format;
	datasetJson["time_start"] 	= time_start;
	datasetJson["time_interval"]= timeIntervalJson;

	std::string fileToOpen = dataset_filename_with_placeholder.replace(placeholderPos, placeholder.length(), time_start);

	GDALDataset *dataset = openGDALDataset(dataset_file_path + "/" + fileToOpen);
	
	datasetJson["coords"]		= readCoords(dataset);
	datasetJson["channels"]		= readChannels(dataset);

	Json::Value provenanceJson(Json::ValueType::objectValue);
	provenanceJson["citation"] 	= citation;
	provenanceJson["license"] 	= license;
	provenanceJson["uri"] 		= uri;
	datasetJson["provenance"] 	= provenanceJson;

	GDALClose(dataset);
	

	//save json to datasetpath with datasetname
	Json::StyledWriter writer;	
	std::ofstream file;
	file.open(datasetJsonPath + dataset_name + ".json");
	file << writer.write(datasetJson);		
	file.close();

}

Json::Value DatasetImporter::readCoords(GDALDataset *dataset){
	Json::Value coordsJson(Json::ValueType::objectValue);

	double adfGeoTransform[6];

	if( dataset->GetGeoTransform( adfGeoTransform ) != CE_None ) {
		GDALClose(dataset);
		throw ImporterException("GDAL Source: No GeoTransform information in raster");
	}

	Json::Value originJson(Json::ValueType::arrayValue);
	originJson.append(adfGeoTransform[0]);
	originJson.append(adfGeoTransform[3]);

	Json::Value scaleJson(Json::ValueType::arrayValue);
	scaleJson.append(adfGeoTransform[1]);
	scaleJson.append(adfGeoTransform[5]);

	Json::Value sizeJson(Json::ValueType::arrayValue);
	sizeJson.append(dataset->GetRasterXSize());
	sizeJson.append(dataset->GetRasterYSize());

	coordsJson["epsg"] = dataset->GetProjectionRef();	//this is an internal string, direct write into the json should be okay. Alternative: GetGCPProjection(), dont know the difference
	coordsJson["origin"] = originJson;
	coordsJson["scale"] = scaleJson;
	coordsJson["size"] = sizeJson;

	return coordsJson;
}

Json::Value DatasetImporter::readChannels(GDALDataset *dataset){
	Json::Value channelsJson(Json::ValueType::arrayValue);
	
	int channelCount = dataset->GetRasterCount();

	for(int i = 1 ; i <= channelCount; i++){
		Json::Value channelJson(Json::ValueType::objectValue);
		//GDALRasterBand is not to be freed, is owned by GDALDataset that will be closed later
		GDALRasterBand *raster = dataset->GetRasterBand(i);

		int success;
		Json::Value unitJson;
		
		unitJson["interpolation"] 	= "unknown";
		unitJson["measurement"] 	= "unknown";
		
		GDALDataType dataType 		= raster->GetRasterDataType();
		channelJson["datatype"] 	= dataTypeToString(dataType);
		
		double minimum 				= raster->GetMinimum(&success);
		if(!success)
			minimum 				= 0.0;
		unitJson["min"] 			= minimum;

		double maximum 				= raster->GetMaximum(&success);
		if(!success)
			maximum = 254.0;
		unitJson["max"] 			= maximum;		

		double nodata = raster->GetNoDataValue(&success);
		if(!success)
			nodata = 255.0;
		channelJson["nodata"] 		= nodata;

		channelJson["unit"]			= unitJson;

		channelsJson.append(channelJson);
	}

	return channelsJson;
}

std::string DatasetImporter::dataTypeToString(GDALDataType type){
	switch(type){
		case GDT_Byte:
			return "Byte";
		case GDT_UInt16:
			return "UInt16";
		case GDT_Int16:
			return "Int16";
		case GDT_UInt32:
			return "UInt32";
		case GDT_Int32:
			return "Int32";
		case GDT_Float32:
			return "Float32";
		case GDT_Float64:
			return "Float64";
		case GDT_CInt16:
			return "CInt16";
		case GDT_CInt32:
			return "CInt32";
		case GDT_CFloat32:
			return "CFloat32";
		case GDT_CFloat64:
			return "CFloat64";
		default:
		case GDT_Unknown:
			return "Unknown";
	}
}

/* ndvi.json
{
   "channels" : [
      {
         "datatype" : "Byte",
         "nodata" : 255.0,
         "unit" : {
            "interpolation" : "unknown",
            "max" : 254.0,
            "measurement" : "unknown",
            "min" : 0.0,
            "unit" : "unknown"
         }
      }
   ],
   "coords" : {
      "epsg" : 4326
      "origin" : [ -180.0, -90 ],
      "scale" : [ 0.10, 0.10 ],
      "size" : [ 3600, 1800 ]
   },
   "provenance": {
   	"citation": "NVDI, Images by Reto Stockli, NASA's Earth Observatory Group, using data provided by the MODIS Land Science Team.",
   	"license": "public domain",
   	"uri": "http://neo.sci.gsfc.nasa.gov/view.php?datasetId=MOD13A2_E_NDVI"
   }
}
*/