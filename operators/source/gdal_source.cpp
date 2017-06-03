#include "datatypes/raster.h"
#include "datatypes/raster/typejuggling.h"
#include "rasterdb/rasterdb.h"
#include "raster/opencl.h"
#include "operators/operator.h"
#include "util/configuration.h"

#include "datatypes/raster/raster_priv.h"
#include "util/gdal.h"

#include <memory>
#include <sstream>
#include <cmath>
#include <json/json.h>


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
	return loadDataset(sourcename, channel, rect.epsg, true, rect);
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
	if (!success /*|| nodata < adfMinMax[0] || nodata > adfMinMax[1]*/) {
		hasnodata = false;
		nodata = 0;
	}

	// Figure out the data type
	epsg_t epsg = default_epsg;
	double minvalue = adfMinMax[0];
	double maxvalue = adfMinMax[1];

	//if (type == GDT_Byte) maxvalue = 255;
	if(epsg == EPSG_GEOSMSG){
		hasnodata = true;
		nodata = 0;
		type = GDT_Int16; // TODO: sollte GDT_UInt16 sein!
	}

	int   nXSize = poBand->GetXSize();
	int   nYSize = poBand->GetYSize();

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

	double x1 = origin_x + scale_x * (pixel_x1 - 0.5) + 0.05;
	double y1 = origin_y + scale_y * (pixel_y1 - 0.5) - 0.05;
	double x2 = x1 + scale_x * pixel_width;
	double y2 = y1 + scale_y * pixel_height - 0.05;

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
		throw ImporterException("GDAL: RasterIO failed");

	return raster;
}

std::unique_ptr<GenericRaster> RasterGDALSourceOperator::loadDataset(std::string filename, int rasterid, epsg_t epsg, bool clip, const QueryRectangle &qrect) {
	GDAL::init();

	GDALDataset *dataset = (GDALDataset *) GDALOpen(filename.c_str(), GA_ReadOnly);

	if (dataset == NULL)
		throw ImporterException(concat("Could not open dataset ", filename));

	double adfGeoTransform[6];

	// http://www.gdal.org/classGDALDataset.html#af9593cc241e7d140f5f3c4798a43a668
	if( dataset->GetGeoTransform( adfGeoTransform ) != CE_None ) {
		GDALClose(dataset);
		throw ImporterException("no GeoTransform information in raster");
	}

	printf("GeoTransform: %f, %f, %f, %f, %f, %f \n", adfGeoTransform[0], adfGeoTransform[1], adfGeoTransform[2], adfGeoTransform[3], adfGeoTransform[4], adfGeoTransform[5], adfGeoTransform[6] );

	int rastercount = dataset->GetRasterCount();

	if (rasterid < 1 || rasterid > rastercount) {
		GDALClose(dataset);
		throw ImporterException("rasterid not found");
	}

	auto raster = loadRaster(dataset, rasterid, adfGeoTransform[0], adfGeoTransform[3], adfGeoTransform[1], adfGeoTransform[5], epsg, clip, qrect.x1, qrect.y1, qrect.x2, qrect.y2, qrect);

	GDALClose(dataset);

	return raster;
}

