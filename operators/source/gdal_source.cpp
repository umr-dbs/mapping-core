#include "datatypes/raster.h"
#include "datatypes/raster/typejuggling.h"
#include "rasterdb/rasterdb.h"
#include "raster/opencl.h"
#include "operators/operator.h"
#include "util/configuration.h"
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
#include <util/gdal_source_datasets.h>

/**
 * Operator that loads raster data via gdal. Loads them from imported GDAL dataset, import via GDAL dataset importer.
 * Parameters:
 * 		sourcename: the name of the imported gdal dataset.
 *		channel:	which channel is to be loaded (channels are 1 based)
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
};


RasterGDALSourceOperator::RasterGDALSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params) : GenericOperator(sourcecounts, sources) {
	//assumeSources(0);
	sourcename = params.get("sourcename", "").asString();
	if (sourcename.length() == 0)
		throw OperatorException("SourceOperator: missing sourcename");
	channel = params.get("channel", 1).asInt();
	datasetPath = Configuration::get("gdalsource.datasets.path");
}

RasterGDALSourceOperator::~RasterGDALSourceOperator(){

}

REGISTER_OPERATOR(RasterGDALSourceOperator, "gdal_source");

void RasterGDALSourceOperator::getProvenance(ProvenanceCollection &pc) {
	std::string local_identifier = "data.gdal_source." + sourcename;
	Json::Value datasetJson = GDALSourceDataSets::getDataSetDescription(sourcename);

	Json::Value provenanceinfo = datasetJson["provenance"];
	if (provenanceinfo.isObject()) {
		pc.add(Provenance(provenanceinfo.get("citation", "").asString(),
								provenanceinfo.get("license", "").asString(),
								provenanceinfo.get("uri", "").asString(),
								local_identifier));
	} else {
		pc.add(Provenance("", "", "", local_identifier));
	}	
}

void RasterGDALSourceOperator::writeSemanticParameters(std::ostringstream &stream) {
	stream << "{\"sourcename\": \"" << sourcename << "\",";
	stream << " \"channel\": " << channel << "}";
}

// load the json definition of the dataset, then get the file to be loaded from GDALTimesnap. Finally load the raster.
std::unique_ptr<GenericRaster> RasterGDALSourceOperator::getRaster(const QueryRectangle &rect, const QueryTools &tools) {
	Json::Value datasetJson = GDALSourceDataSets::getDataSetDescription(sourcename);
	std::string file_path 	= GDALTimesnap::getDatasetFilename(datasetJson, rect.t1);	
	auto raster 			= loadDataset(file_path, channel, rect.epsg, true, rect);	
	//flip here so the tiff result will not be flipped
	return raster->flip(false, true);
}

// loads the raster and read the wanted raster data section into a GenericRaster
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

	//calculate query rectangle in pixels of the source raster TODO: reuse functionality from RasterDB/GDALCRS
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

        pixel_width = pixel_x2 - pixel_x1 + 1;
		pixel_height = pixel_y2 - pixel_y1 + 1;
	}

	double x1 = origin_x + scale_x * pixel_x1;
	double y1 = origin_y + scale_y * pixel_y1;
	double x2 = x1 + scale_x * pixel_width;
	double y2 = y1 + scale_y * pixel_height;

	if (x1 > x2)
		std::swap(x1, x2);
	if (y1 > y2)
		std::swap(y1, y2);

	// Read Pixel data with GDAL
    // limit to (0, width), (0, height) for gdal
    int gdal_pixel_x1 = std::max(0, pixel_x1);
    int gdal_pixel_y1 = std::max(0, pixel_y1);
    int gdal_pixel_offset_x = gdal_pixel_x1 - pixel_x1;
    int gdal_pixel_width = pixel_width - gdal_pixel_offset_x;
    int gdal_pixel_offset_y = gdal_pixel_y1 - pixel_y1;
    int gdal_pixel_height = pixel_height - gdal_pixel_offset_y;
    gdal_pixel_width = std::min(nXSize - gdal_pixel_x1, gdal_pixel_width);
    gdal_pixel_height = std::min(nYSize - gdal_pixel_y1, gdal_pixel_height);

	// compute the rectangle for the gdal raster
	double gdal_x1 = origin_x + scale_x * gdal_pixel_x1;
	double gdal_y1 = origin_y + scale_y * gdal_pixel_y1;
	double gdal_x2 = gdal_x1 + scale_x * gdal_pixel_width;
	double gdal_y2 = gdal_y1 + scale_y * gdal_pixel_height;


	const TemporalReference &tref = (const TemporalReference &)qrect;
	SpatioTemporalReference stref(
			SpatialReference(qrect.epsg, gdal_x1, gdal_y1, gdal_x2, gdal_y2, flipx, flipy),
			TemporalReference(tref.timetype, tref.t1, tref.t2)
	);
	Unit unit = Unit::unknown();
	unit.setMinMax(minvalue, maxvalue);
	DataDescription dd(type, unit, hasnodata, nodata);

	// read the actual data
	double scale_x_zoomed = (qrect.x2 - qrect.x1) / qrect.xres;
	double scale_y_zoomed = (qrect.y2 - qrect.y1) / qrect.yres;

	double factor_x = std::abs(scale_x / scale_x_zoomed);
	double factor_y = std::abs(scale_y / scale_y_zoomed);

	int gdal_raster_width = static_cast<int> (std::ceil(gdal_pixel_width * factor_x));
	int gdal_raster_height = static_cast<int> (std::ceil(gdal_pixel_height * factor_y));
	auto raster = GenericRaster::create(dd, stref, gdal_raster_width, gdal_raster_height);
	void *buffer = raster->getDataForWriting();

    auto res = poBand->RasterIO(GF_Read,
                                gdal_pixel_x1, gdal_pixel_y1, gdal_pixel_width, gdal_pixel_height,  // rectangle in the source raster
                                buffer, raster->width,  raster->height,  // position and size of the destination buffer
                                type, 0, 0, NULL);

	if (res != CE_None){
		GDALClose(dataset);
		throw OperatorException("GDAL Source: RasterIO failed");
	}


	// check if requested query rectangle exceed the data returned from GDAL
	if (pixel_width > gdal_pixel_width || pixel_height > gdal_pixel_height) {
		// loaded raster has to be filled up with nodata
		SpatioTemporalReference stref(
				SpatialReference(qrect.epsg, x1, y1, x2, y2, flipx, flipy),
				TemporalReference(tref.timetype, tref.t1, tref.t2)
		);

		auto raster2 = GenericRaster::create(dd, stref, qrect.xres, qrect.yres);
		int blit_offset_x = gdal_pixel_offset_x * factor_x;
		int blit_offset_y = gdal_pixel_offset_y * factor_y;
		raster2->blit(raster.get(), blit_offset_x, blit_offset_y);

		return raster2;
	}

	//GDALRasterBand is not to be freed, is owned by GDALDataset that will be closed later
	return raster;
}

//load the GDALDataset from disk
std::unique_ptr<GenericRaster> RasterGDALSourceOperator::loadDataset(std::string filename, int rasterid, epsg_t epsg, bool clip, const QueryRectangle &qrect) {
	
	GDAL::init();

	GDALDataset *dataset = (GDALDataset *) GDALOpen(filename.c_str(), GA_ReadOnly);

	if (dataset == NULL)
		throw OperatorException(concat("GDAL Source: Could not open dataset ", filename));

	//read GeoTransform to get origin and scale
	double adfGeoTransform[6];
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


