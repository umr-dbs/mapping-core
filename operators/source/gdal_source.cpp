#include "datatypes/raster.h"
#include "datatypes/raster/typejuggling.h"
#include "rasterdb/rasterdb.h"
#include "raster/opencl.h"
#include "operators/operator.h"
#include "util/configuration.h"
#include "util/timeparser.h"
#include "util/gdal_timesnap.h"

#include "datatypes/raster/raster_priv.h"
#include "util/gdal.h"

#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cmath>
#include <json/json.h>
#include <time.h>
#include <dirent.h>

/**
 * Operator that gets raster data from file via gdal
 *
 */
class RasterGDALSourceOperator : public GenericOperator {
	public:
		RasterGDALSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);
		virtual ~RasterGDALSourceOperator();

		virtual void getProvenance(ProvenanceCollection &pc);
		virtual std::unique_ptr<GenericRaster> getRaster(const QueryRectangle &rect, const QueryTools &tools);
	protected:
		void writeSemanticParameters(std::ostringstream &stream);
	private:

		std::string datasetPath;

		std::string sourcename;
		int channel;
		bool transform;

		std::unique_ptr<GenericRaster> loadDataset( std::string filename, 
													int rasterid, 													
													epsg_t epsg, 
													bool clip, 
													const QueryRectangle &qrect);
		
		std::unique_ptr<GenericRaster> loadRaster(  GDALDataset *dataset, 
													int rasteridx, double origin_x, 
													double origin_y, 
													double scale_x, double scale_y, 																					   
													epsg_t default_epsg, bool clip, 
													double clip_x1, double clip_y1, 
													double clip_x2, double clip_y2,
													const QueryRectangle &qrect);
		
		Json::Value getDatasetJson(std::string datasetName);
		std::string getDatasetFilename(Json::Value datasetJson, double wantedTimeUnix);
};


RasterGDALSourceOperator::RasterGDALSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params) : GenericOperator(sourcecounts, sources) {
	//assumeSources(0);
	sourcename = params.get("sourcename", "").asString();
	if (sourcename.length() == 0)
		throw OperatorException("SourceOperator: missing sourcename");

	
	channel = params.get("channel", 1).asInt();
	transform = params.get("transform", true).asBool();
	datasetPath = Configuration::get("gdalsource.datasetpath");					
}

RasterGDALSourceOperator::~RasterGDALSourceOperator(){

}

REGISTER_OPERATOR(RasterGDALSourceOperator, "gdal_source");

void RasterGDALSourceOperator::getProvenance(ProvenanceCollection &pc) {
	/*std::string local_identifier = "data." + getType() + "." + sourcename;

	auto sp = rasterdb->getProvenance();
	if (sp == nullptr)
		pc.add(Provenance("", "", "", local_identifier));
	else
		pc.add(Provenance(sp->citation, sp->license, sp->uri, local_identifier));
		*/
}

void RasterGDALSourceOperator::writeSemanticParameters(std::ostringstream &stream) {
	std::string trans = transform ? "true" : "false";
	stream << "{\"sourcename\": \"" << sourcename << "\",";
	stream << " \"channel\": " << channel << ",";
	stream << " \"transform\": " << trans << "}";
}

std::unique_ptr<GenericRaster> RasterGDALSourceOperator::getRaster(const QueryRectangle &rect, const QueryTools &tools) {
	Json::Value datasetJson = getDatasetJson(sourcename);	
	std::string file_path 	= getDatasetFilename(datasetJson, rect.t1);	
	auto raster 			= loadDataset(file_path, channel, rect.epsg, true, rect);	
	//flip here so the tiff result will not be flipped
	return raster->flip(false, true);
}

std::unique_ptr<GenericRaster> RasterGDALSourceOperator::loadRaster(GDALDataset *dataset, int rasteridx, double origin_x, double origin_y, double scale_x, double scale_y, epsg_t default_epsg, bool clip, double clip_x1, double clip_y1, double clip_x2, double clip_y2, const QueryRectangle& qrect) {
	GDALRasterBand  *poBand;
	int             nBlockXSize, nBlockYSize;
	int             bGotMin, bGotMax;
	double          adfMinMax[2];

	bool flipx = false, flipy = false;


	poBand = dataset->GetRasterBand( rasteridx );
	poBand->GetBlockSize( &nBlockXSize, &nBlockYSize );	

	GDALDataType type = poBand->GetRasterDataType();


	adfMinMax[0] = poBand->GetMinimum( &bGotMin );
	adfMinMax[1] = poBand->GetMaximum( &bGotMax );
	if( ! (bGotMin && bGotMax) )
		GDALComputeRasterMinMax((GDALRasterBandH)poBand, TRUE, adfMinMax);

	int hasnodata = true;
	int success;
	double nodata = poBand->GetNoDataValue(&success);

	if (!success) {
		hasnodata = false;
		nodata = 0;
	}
	
	double minvalue = adfMinMax[0];
	double maxvalue = adfMinMax[1];

	int nXSize = poBand->GetXSize();
	int nYSize = poBand->GetYSize();

	int pixel_x1 = 0;
	int pixel_y1 = 0;
	int pixel_width = nXSize;
	int pixel_height = nYSize;
	if (clip) {
		pixel_x1 = floor((clip_x1 - origin_x) / scale_x);
		pixel_y1 = floor((clip_y1 - origin_y) / scale_y);
		int pixel_x2 = floor((clip_x2 - origin_x) / scale_x);
		int pixel_y2 = floor((clip_y2 - origin_y) / scale_y);

		if (pixel_x1 > pixel_x2)
			std::swap(pixel_x1, pixel_x2);
		if (pixel_y1 > pixel_y2)
			std::swap(pixel_y1, pixel_y2);

		pixel_x1 = std::max(0, pixel_x1);
		pixel_y1 = std::max(0, pixel_y1);

		pixel_x2 = std::min(nXSize-1, pixel_x2);
		pixel_y2 = std::min(nYSize-1, pixel_y2);

		pixel_width = pixel_x2 - pixel_x1 + 1;
		pixel_height = pixel_y2 - pixel_y1 + 1;
	}

	double x1 = origin_x + scale_x * pixel_x1;
	double y1 = origin_y + scale_y * pixel_y1;
	double x2 = x1 + scale_x * pixel_width;
	double y2 = y1 + scale_y * pixel_height;

	const TemporalReference &tref = (const TemporalReference &)qrect;

	SpatioTemporalReference stref(
		SpatialReference(qrect.epsg, x1, y1, x2, y2, flipx, flipy),
		TemporalReference(tref.timetype, tref.t1, tref.t2)		
	);
	Unit unit = Unit::unknown();
	unit.setMinMax(minvalue, maxvalue);
	DataDescription dd(type, unit, hasnodata, nodata);

	auto raster = GenericRaster::create(dd, stref, pixel_width, pixel_height);
	void *buffer = raster->getDataForWriting();

	
	GDALRasterIOExtraArg *extraArg = NULL; //new GDALRasterIOExtraArg;
	//extraArg->nVersion = 0; //without settings this its throwing an "Importer Exception" with "Unhandled version of GDALRasterIOExtraArg" Whit setting it its an segmentation fault
	//extraArg->eResampleAlg = GRIORA_Bilinear;
	

	// Read Pixel data
	auto res = poBand->RasterIO( GF_Read,
		pixel_x1, pixel_y1, pixel_width, pixel_height, // rectangle in the source raster
		buffer, raster->width, raster->height, // position and size of the destination buffer
		type, 0, 0, extraArg);	//NULL instead of extraArg Pointer. &extraArg

	if (res != CE_None)
		throw OperatorException("GDAL Source: RasterIO failed");

	//GDALRasterBand is not to be freed, is owned by GDALDataset that will be closed later
	return raster;
}

std::unique_ptr<GenericRaster> RasterGDALSourceOperator::loadDataset(std::string filename, int rasterid, epsg_t epsg, bool clip, const QueryRectangle &qrect) {
	
	GDAL::init();

	GDALDataset *dataset = (GDALDataset *) GDALOpen(filename.c_str(), GA_ReadOnly);

	if (dataset == NULL)
		throw OperatorException(concat("GDAL Source: Could not open dataset ", filename));

	double adfGeoTransform[6];

	// http://www.gdal.org/classGDALDataset.html#af9593cc241e7d140f5f3c4798a43a668
	if( dataset->GetGeoTransform( adfGeoTransform ) != CE_None ) {
		GDALClose(dataset);
		throw OperatorException("GDAL Source: No GeoTransform information in raster");
	}

	int rastercount = dataset->GetRasterCount();

	if (rasterid < 1 || rasterid > rastercount) {
		GDALClose(dataset);
		throw OperatorException("GDAL Source: rasterid not found");
	}

	auto raster = loadRaster(dataset, rasterid, adfGeoTransform[0], adfGeoTransform[3], adfGeoTransform[1], adfGeoTransform[5], epsg, clip, qrect.x1, qrect.y1, qrect.x2, qrect.y2, qrect);

	GDALClose(dataset);

	return raster;
}

std::string RasterGDALSourceOperator::getDatasetFilename(Json::Value datasetJson, double wantedTimeUnix){

	std::string time_format = datasetJson.get("time_format", "%Y-%m-%d").asString();
	std::string time_start 	= datasetJson.get("time_start", "0").asString();
	std::string time_end 	= datasetJson.get("time_end", "0").asString();
    
    auto timeParser = TimeParser::createCustom(time_format); 

    //check if requested time is in range of dataset timestamps
	double startUnix 	= timeParser->parse(time_start);
	//double endUnix 		= timeParser->parse(time_end);
	
	if(wantedTimeUnix < startUnix){		// || (endUnix > 0 && wantedTimeUnix > endUnix)){
		throw NoRasterForGivenTimeException("Requested time is not in range of dataset");
	}
	Json::Value timeInterval = datasetJson.get("time_interval", NULL);

	TimeUnit intervalUnit 	= GDALTimesnap::createTimeUnit(timeInterval.get("unit", "Month").asString());
	int intervalValue 		= timeInterval.get("value", 1).asInt();
	
	//std::cout << "Interval unit: " << (int)intervalUnit << ", interval value: " << intervalValue << std::endl;

	if(intervalUnit != TimeUnit::Year){
		if(GDALTimesnap::maxValueForTimeUnit(intervalUnit) % intervalValue != 0){
			throw OperatorException("GDALSource: dataset invalid, interval value modulo unit-length must be 0. e.g. 4 % 12, for Months, or 30%60 for Minutes");
		}
	} else {
		if(intervalValue != 1){
			throw OperatorException("GDALSource: dataset invalid, interval value for interval unit Year has to be 1.");	
		}
	}

	time_t wantedTimeTimet = wantedTimeUnix;	
	tm wantedTimeTm;
	gmtime_r(&wantedTimeTimet, &wantedTimeTm);
	std::cout << "WantedTime: " << GDALTimesnap::unixTimeToString(wantedTimeUnix, time_format) << std::endl;

	time_t startTimeTimet = startUnix;	
	tm startTimeTm;
	gmtime_r(&startTimeTimet, &startTimeTm);
	std::cout << "StartTime: " << GDALTimesnap::unixTimeToString(startUnix, time_format) << std::endl;

	tm snappedTime = GDALTimesnap::snapToInterval(intervalUnit, intervalValue, startTimeTm, wantedTimeTm);
	
	// get string of snapped time and put the file path, name together
	
	std::string snappedTimeString  = GDALTimesnap::tmStructToString(&snappedTime, time_format);
	std::cout << "Snapped Time: " << snappedTimeString << std::endl;

	std::string path = datasetJson.get("path", "").asString();
	std::string fileName = datasetJson.get("file_name", "").asString();

	std::string placeholder = "%%%TIME_STRING%%%";
	size_t placeholderPos = fileName.find(placeholder);

	fileName = fileName.replace(placeholderPos, placeholder.length(), snappedTimeString);
	return path + "/" + fileName;
}



Json::Value RasterGDALSourceOperator::getDatasetJson(std::string datasetName){
	// opens the standard path for datasets and returns the dataset with the name datasetName as Json::Value
	struct dirent *entry;
	DIR *dir = opendir(datasetPath.c_str());

	if (dir == NULL) {
        throw OperatorException("GDAL Source: directory for dataset json files does not exist.");
    }

	while ((entry = readdir(dir)) != NULL) {
        std::string filename = entry->d_name;
        std::string withoutExtension = filename.substr(0, filename.length() - 5);
        
        if(withoutExtension == datasetName){
        	
        	//open file then read json object from it
        	std::ifstream file(datasetPath + filename);
			if (!file.is_open()) {
			    closedir(dir);
				throw OperatorException("GDAL Source Operator: unable to dataset file " + datasetName);
			}

			Json::Reader reader(Json::Features::strictMode());
			Json::Value json;
			if (!reader.parse(file, json)) {
			    closedir(dir);
				throw OperatorException("GDAL Source Operator: unable to read json" + reader.getFormattedErrorMessages());				
			}				

		    closedir(dir);
        	return json;
        }

    }

    closedir(dir);
    throw OperatorException("GDAL Source: Dataset " + datasetName + " does not exist.");
}