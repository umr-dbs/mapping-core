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
		int minValueForTimeUnit(TimeUnit part) const;
		std::string tmStructToString(tm *tm, std::string format);
		std::string unixTimeToString(double unix_time, std::string format);
		tm tmDifference(tm &first, tm &second);
		int getUnitDifference(tm diff, TimeUnit snapUnit);		
		void setTimeUnitValueInTm(tm &time, TimeUnit unit, int value);
		int getTimeUnitValueFromTm(tm &time, TimeUnit unit);
		void handleOverflow(tm &snapped, TimeUnit intervalUnit);


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
		tm snapToInterval(RasterGDALSourceOperator::TimeUnit unit, int unitValue, tm startTime, tm wantedTime);		
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
	
	std::string time_format = datasetJson.get("time_format", "%Y-%m-%d").asString();
	std::string time_start 	= datasetJson.get("time_start", "0").asString();
	std::string time_end 	= datasetJson.get("time_end", "0").asString();
    
    auto timeParser = TimeParser::createCustom(time_format); 

    //check if requested time is in range of dataset timestamps
	double startUnix 	= timeParser->parse(time_start);
	//double endUnix 		= timeParser->parse(time_end);
	
	if(wantedTimeUnix < startUnix){		// || (endUnix > 0 && wantedTimeUnix > endUnix)){
		throw OperatorException("Requested time is not in range of dataset");
	}

	Json::Value time_interval = datasetJson.get("time_interval", NULL);

	TimeUnit interval_unit 	= createTimeUnit(time_interval.get("unit", "Month").asString());
	int interval_value 		= time_interval.get("value", 1).asInt();
	
	//std::cout << "Interval unit: " << (int)interval_unit << ", interval value: " << interval_value << std::endl;

	if(interval_unit != TimeUnit::Year){
		if(maxValueForTimeUnit(interval_unit) % interval_value != 0){
			throw OperatorException("GDALSource: dataset invalid, interval value modulo unit-length must be 0. e.g. 4 % 12, for Months, or 30%60 for Minutes");
		}
	} else {
		if(interval_value != 1){
			throw OperatorException("GDALSource: dataset invalid, interval value for interval unit Year has to be 1.");	
		}
	}

	time_t wantedTimeTimet = wantedTimeUnix;	
	tm wantedTimeTm = *(gmtime(&wantedTimeTimet));
	//std::cout << "WantedTime: " << unixTimeToString(wantedTimeUnix, time_format) << std::endl;

	time_t startTimeTimet = startUnix;	
	tm startTimeTm = *(gmtime(&startTimeTimet));
	//std::cout << "StartTime: " << unixTimeToString(startUnix, time_format) << std::endl;

	tm snapped_time = snapToInterval(interval_unit, interval_value, startTimeTm, wantedTimeTm);
	
	// get string of snapped time and put the file path, name together
	
	std::string snapped_time_string  = tmStructToString(&snapped_time, time_format);
	//std::cout << "Snapped Time: " << snapped_time_string << std::endl;

	std::string path = datasetJson.get("path", "").asString();
	std::string file_name = datasetJson.get("file_name", "").asString();

	std::string placeholder = "%%%TIME_STRING%%%";
	size_t placeholer_pos = file_name.find(placeholder);

	file_name = file_name.replace(placeholer_pos, placeholder.length(), snapped_time_string);
	return path + "/" + file_name;
}

tm RasterGDALSourceOperator::snapToInterval(RasterGDALSourceOperator::TimeUnit snapUnit, int intervalValue, tm startTime, tm wantedTime){
	
	tm diff 	= tmDifference(wantedTime, startTime);
	tm snapped 	= startTime;

	int unitDiffValue 	= getUnitDifference(diff, snapUnit);
	int unitDiffModulo 	= unitDiffValue % intervalValue;	
	unitDiffValue 		-= unitDiffModulo; 

	if(snapUnit == TimeUnit::Day)	//because day is the only value in tm struct that is not 0 based
		unitDiffValue -= 1;

	// add the units difference on the start value
	setTimeUnitValueInTm(snapped, snapUnit, getTimeUnitValueFromTm(snapped, snapUnit) + unitDiffValue);

	// handle the created overflow
	handleOverflow(snapped, snapUnit);

	return snapped;
}

int RasterGDALSourceOperator::getUnitDifference(tm diff, TimeUnit snapUnit){
	const int snapUnitAsInt = (int)snapUnit;	
	int unitDiff = getTimeUnitValueFromTm(diff, snapUnit);

	if(snapUnitAsInt > 0){
		// add all the bigger time units from diff together as the one unit above the snapUnit. eg 1 year -> 12 month
		for(int i = 0; i < snapUnitAsInt - 1; i++){
			TimeUnit tu = (TimeUnit)i;
			int val = getTimeUnitValueFromTm(diff, tu);
			if(val > 0){
				TimeUnit nextTu = (TimeUnit)(i+1);
				int newValue = getTimeUnitValueFromTm(diff, nextTu) + val * maxValueForTimeUnit(nextTu);
				setTimeUnitValueInTm(diff, nextTu, newValue);
			}
		}		
		TimeUnit unitBefore = (TimeUnit)(snapUnitAsInt - 1);
		int valueBefore = getTimeUnitValueFromTm(diff, unitBefore);		
		unitDiff += valueBefore * maxValueForTimeUnit(snapUnit);
	}

	//if one of the smaller time units than snapUnit is negative -> unitdiff -= 1 because one part of the difference was not a whole unit
	for(int i = snapUnitAsInt+1; i <= (int)TimeUnit::Second; i++){
		TimeUnit tu = (TimeUnit)i;		
		if(getTimeUnitValueFromTm(diff, tu) < minValueForTimeUnit(tu)){
			unitDiff -= 1;
			break;
		}
	}

	return unitDiff;
}

void RasterGDALSourceOperator::handleOverflow(tm &snapped, TimeUnit snapUnit){
	const int snapUnitAsInt = (int)snapUnit;
	
	for(int i = snapUnitAsInt; i > 0; i--){
		TimeUnit tu = (TimeUnit)i;
		int value = getTimeUnitValueFromTm(snapped, tu);
		int maxValue = maxValueForTimeUnit(tu);
		if(tu == TimeUnit::Day)
			maxValue += 1;	//because day is 1 based, and next value > maxValue-1 is checked

		if(value > maxValue - 1){
			TimeUnit tuBefore = (TimeUnit)(i-1);
			int mod = value % maxValue;
			int div = value / maxValue;			
			setTimeUnitValueInTm(snapped, tu, mod);			
			setTimeUnitValueInTm(snapped, tuBefore, getTimeUnitValueFromTm(snapped, tuBefore) + div);						
		}
	}	

}

void RasterGDALSourceOperator::setTimeUnitValueInTm(tm &time, TimeUnit unit, int value){
	switch(unit){
		case RasterGDALSourceOperator::TimeUnit::Year:
			time.tm_year = value;
			return;
		case RasterGDALSourceOperator::TimeUnit::Month:
			time.tm_mon = value;
			return;
		case RasterGDALSourceOperator::TimeUnit::Day:
			time.tm_mday = value;
			return;
		case RasterGDALSourceOperator::TimeUnit::Hour:
			time.tm_hour = value;
			return;
		case RasterGDALSourceOperator::TimeUnit::Minute:
			time.tm_min = value;
			return;
		case RasterGDALSourceOperator::TimeUnit::Second:
			time.tm_sec = value;
			return;
	}	
}

int RasterGDALSourceOperator::getTimeUnitValueFromTm(tm &time, TimeUnit unit){	
	switch(unit){
		case RasterGDALSourceOperator::TimeUnit::Year:
			return time.tm_year;
		case RasterGDALSourceOperator::TimeUnit::Month:
			return time.tm_mon;
		case RasterGDALSourceOperator::TimeUnit::Day:
			return time.tm_mday;
		case RasterGDALSourceOperator::TimeUnit::Hour:
			return time.tm_hour;
		case RasterGDALSourceOperator::TimeUnit::Minute:
			return time.tm_min;
		case RasterGDALSourceOperator::TimeUnit::Second:
			return time.tm_sec;
	}
}

Json::Value RasterGDALSourceOperator::getDatasetJson(std::string datasetName){
	// opens the standard path for datasets and returns the dataset with the name datasetName as Json::Value
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

int RasterGDALSourceOperator::minValueForTimeUnit(TimeUnit part) const {
	// based on tm struct: see http://www.cplusplus.com/reference/ctime/tm/ for value ranges of tm struct
	if(part == TimeUnit::Day)
		return 1;
	else 
		return 0;
}

int RasterGDALSourceOperator::maxValueForTimeUnit(TimeUnit part) const {
	switch(part){
		case TimeUnit::Year:
			return 0;
		case TimeUnit::Month:
			return 12;			
		case TimeUnit::Day:	//TODO: how to handle 31, 28 day months?
			return 30;			
		case TimeUnit::Hour:
			return 24;
		case TimeUnit::Minute:
			return 60;
		case TimeUnit::Second:
			return 60;
	}
}

std::string RasterGDALSourceOperator::tmStructToString(tm *tm, std::string format){	
	char date[20];	//max length of a time string is 19 + zero termination
	strftime(date, sizeof(date), format.c_str(), tm);
	return std::string(date);
}

std::string RasterGDALSourceOperator::unixTimeToString(double unix_time, std::string format){
	time_t tt = unix_time;
	struct tm *tm = gmtime(&tt);
	char date[20];	//max length of a time string is 19 + zero termination
	strftime(date, sizeof(date), format.c_str(), tm);
	return std::string(date);
}

tm RasterGDALSourceOperator::tmDifference(tm &first, tm &second){
	// igonres tm_wday and tm_yday and tm_isdst, because they are not needed
	tm diff;
	diff.tm_year 	= first.tm_year - second.tm_year;
	diff.tm_mon 	= first.tm_mon - second.tm_mon;
	diff.tm_mday 	= first.tm_mday - second.tm_mday;
	diff.tm_hour 	= first.tm_hour - second.tm_hour;
	diff.tm_min 	= first.tm_min - second.tm_min;
	diff.tm_sec 	= first.tm_sec - second.tm_sec;

	return diff;
}
