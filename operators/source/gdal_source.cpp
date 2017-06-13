#include "datatypes/raster.h"
#include "datatypes/raster/typejuggling.h"
#include "rasterdb/rasterdb.h"
#include "raster/opencl.h"
#include "operators/operator.h"
#include "util/configuration.h"
#include "util/timeparser.h"

#include "datatypes/raster/raster_priv.h"
#include "util/gdal.h"

#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cmath>
#include <json/json.h>
#include <ctime>
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

		const std::string DATASET_PATH = "test/systemtests/data/gdal_files";

		enum class TimeUnit {
		    Year 	= 0,
		    Month 	= 1,
		    Day 	= 2,
		    Hour 	= 3,
			Minute  = 4,
		    Second  = 5
		};
		
		static const std::map<std::string, RasterGDALSourceOperator::TimeUnit> string_to_TimeUnit;
		static TimeUnit createTimeUnit(std::string value);


		int maxValueForTimeUnit(TimeUnit part) const;
		std::string unixTimeToString(double unix_time, std::string format);

		std::string sourcename;
		int channel;
		bool transform;

		std::unique_ptr<GenericRaster> loadDataset( std::string filename, 
													int rasterid, 													
													epsg_t epsg, 
													bool clip, 
													const QueryRectangle &qrect);
		
		std::unique_ptr<GenericRaster> loadRaster(GDALDataset *dataset, int rasteridx, double origin_x, double origin_y, 
																					   double scale_x, double scale_y, 																					   
																					   epsg_t default_epsg, bool clip, 
																					   double clip_x1, double clip_y1, 
																					   double clip_x2, double clip_y2,
																					   const QueryRectangle &qrect);
		Json::Value getDatasetJson(std::string datasetName);
		std::string getDatasetFilename(Json::Value datasetJson, double wantedTimeUnix);
		std::string snapToInterval(RasterGDALSourceOperator::TimeUnit unit, int unitValue, std::string time, std::string format);		
};


RasterGDALSourceOperator::RasterGDALSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params) : GenericOperator(sourcecounts, sources) {
	//assumeSources(0);
	sourcename = params.get("sourcename", "").asString();
	if (sourcename.length() == 0)
		throw OperatorException("SourceOperator: missing sourcename");

	
	channel = params.get("channel", 1).asInt();
	transform = params.get("transform", true).asBool();
	
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
	std::string file_path = getDatasetFilename(datasetJson, rect.t1);
	auto raster = loadDataset(file_path, channel, rect.epsg, true, rect);	
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

	// ISO 8601 timestamp: 2017-06-12T07:40:16Z	
	
	std::string time_format = datasetJson.get("time_format", "%Y-%m-%d").asString();
	std::string time_start 	= datasetJson.get("time_start", "").asString();
	std::string time_end 	= datasetJson.get("time_end", "").asString();
    
    auto timeParser = TimeParser::createCustom(time_format); 

    //check if requested time is in range of dataset timestamps
	double startUnix 	= timeParser->parse(time_start);
	double endUnix 		= timeParser->parse(time_end);
	if(wantedTimeUnix < startUnix || wantedTimeUnix > endUnix){
		throw new OperatorException("Requested time is not in range of dataset");
	}

	Json::Value time_interval = datasetJson.get("time_interval", NULL);

	TimeUnit interval_unit = createTimeUnit(time_interval.get("unit", "Month").asString());
	int interval_value = time_interval.get("value", 1).asInt();
	
	if(interval_unit != TimeUnit::Year){
		std::cout << maxValueForTimeUnit(interval_unit) % interval_value << std::endl;
		if(maxValueForTimeUnit(interval_unit) % interval_value != 0){
			throw OperatorException("GDALSource: dataset invalid, interval value modulo unit-length must be 0. e.g. 4 % 12, for Months, or 30%60 for Minutes");
		}
	} else {
		if(interval_value != 1){
			throw OperatorException("GDALSource: dataset invalid, interval value for interval unit Year has to be 1.");	
		}
	}

	std::string wantedTimeFormated = unixTimeToString(wantedTimeUnix, time_format); //do i need this?
	time_t wantedTimeTimet = startUnix;
	tm wantedTimeTm = *gmtime(&wantedTimeUnix);

	time_t startTimeTimet = startUnix;
	tm startTimeTm = *gmtime(&startTimeTimet);


	tm snapped_time = snapToInterval(interval_unit, interval_value, startTimeTm, wantedTimeTm);

	// todo: snapped time to string, with tm -> time_t -> unixToString

	std::string path = datasetJson.get("path", "").asString();
	std::string file_name = datasetJson.get("file_name", "").asString();

	std::string placeholder = "%%%TIME_STRING%%%";
	size_t placeholer_pos = file_name.find(placeholder);

	file_name = file_name.replace(placeholer_pos, placeholder.length(), snapped_time_string);
	return path + "/" + file_name;
}

tm RasterGDALSourceOperator::snapToInterval(RasterGDALSourceOperator::TimeUnit unit, int unitValue, tm startTime, tm wantedTime){
	tm snapped;

	// TODO
	
	return snapped;
}

Json::Value RasterGDALSourceOperator::getDatasetJson(std::string datasetName){
	struct dirent *entry;
	DIR *dir = opendir(DATASET_PATH.c_str());

	if (dir == NULL) {
        throw OperatorException("GDAL Source: directory for dataset json files does not exist.");
    }

	while ((entry = readdir(dir)) != NULL) {
        std::string filename = entry->d_name;
        std::string withoutExtension = filename.substr(0, filename.length() - 5);
        
        if(withoutExtension == datasetName){
        	
        	//open file then read json object from it
        	std::ifstream file(DATASET_PATH + "/" + filename);
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

const std::map<std::string, RasterGDALSourceOperator::TimeUnit> RasterGDALSourceOperator::string_to_TimeUnit {
	{"Second", 	RasterGDALSourceOperator::TimeUnit::Second},
	{"Minute", 	RasterGDALSourceOperator::TimeUnit::Minute},
	{"Hour", 	RasterGDALSourceOperator::TimeUnit::Hour},
	{"Day", 	RasterGDALSourceOperator::TimeUnit::Day},
	{"Month", 	RasterGDALSourceOperator::TimeUnit::Month},
	{"Year", 	RasterGDALSourceOperator::TimeUnit::Year}
};

RasterGDALSourceOperator::TimeUnit RasterGDALSourceOperator::createTimeUnit(std::string value) {
	return string_to_TimeUnit.at(value);
}

int RasterGDALSourceOperator::maxValueForTimeUnit(RasterGDALSourceOperator::TimeUnit part) const {
	switch(part){
		case RasterGDALSourceOperator::TimeUnit::Year:
			return 0;
		case RasterGDALSourceOperator::TimeUnit::Month:
			return 12;			
		case RasterGDALSourceOperator::TimeUnit::Day:	//TODO: how to handle 31, 28 day months?
			return 30;			
		case RasterGDALSourceOperator::TimeUnit::Hour:
			return 24;
		case RasterGDALSourceOperator::TimeUnit::Minute:
			return 60;
		case RasterGDALSourceOperator::TimeUnit::Second:
			return 60;
	}
}

std::string RasterGDALSourceOperator::unixTimeToString(double unix_time, std::string format){
	time_t tt = unix_time;
	time_t t = time(&tt);
	struct tm *tm = localtime(&t);
	char date[20];	//max length of a time string is 19 + zero escape
	strftime(date, sizeof(date), format.c_str(), tm);
	return std::string(date);
}
