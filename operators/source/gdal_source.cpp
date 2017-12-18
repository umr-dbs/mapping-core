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

		std::unique_ptr<GenericRaster> loadDataset( const GDALTimesnap::GDALDataLoadingInfo &loadingInfo,
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
													const QueryRectangle &qrect,
													const TemporalReference &tref,
                                                    const GDALTimesnap::GDALDataLoadingInfo &loadingInfo);
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
	GDALTimesnap::GDALDataLoadingInfo loadingInfo = GDALTimesnap::getDataLoadingInfo(datasetJson, channel, rect);
	auto raster = loadDataset(loadingInfo, rect.epsg, true, rect);
	//flip here so the tiff result will not be flipped
	return raster->flip(false, true);
}

bool overlaps (double a_start, double a_end, double b_start, double b_end) {
	return a_end > a_start && b_end > b_start && a_end >= b_start && a_start <= b_end;
}

// loads the raster and read the wanted raster data section into a GenericRaster
std::unique_ptr<GenericRaster> RasterGDALSourceOperator::loadRaster(GDALDataset *dataset, int rasteridx, double origin_x,
																	double origin_y, double scale_x, double scale_y,
																	epsg_t default_epsg, bool clip, double clip_x1,
																	double clip_y1, double clip_x2, double clip_y2,
																	const QueryRectangle& qrect, const TemporalReference &tref,
																	const GDALTimesnap::GDALDataLoadingInfo &loadingInfo) {
	// get raster metadata
    GDALRasterBand  *poBand;
	int             nBlockXSize, nBlockYSize;
	int             bGotMin, bGotMax;
	double          adfMinMax[2];

	bool flipx = false, flipy = false;

	poBand = dataset->GetRasterBand( rasteridx );
	poBand->GetBlockSize( &nBlockXSize, &nBlockYSize );	

	GDALDataType type = poBand->GetRasterDataType();

    bool hasnodata;
    double nodata;

    if (std::isnan(loadingInfo.nodata)) {
        int success;
        nodata = poBand->GetNoDataValue(&success);

        if (success) {
            hasnodata = true;
        } else {
            hasnodata = false;
        }
    } else {
        hasnodata = true;
        nodata = loadingInfo.nodata;
    }
	
	int nXSize = poBand->GetXSize();
	int nYSize = poBand->GetYSize();

	//calculate query rectangle in pixels of the source raster TODO: reuse functionality from RasterDB/GDALCRS
	int pixel_x1 = 0;
	int pixel_y1 = 0;
    int pixel_x2 = nXSize - 1;
    int pixel_y2 = nYSize - 1;
	int pixel_width = nXSize;
	int pixel_height = nYSize;
	if (clip) {
		pixel_x1 = static_cast<int>(floor((clip_x1 - origin_x) / scale_x));
		pixel_y1 = static_cast<int>(floor((clip_y1 - origin_y) / scale_y));
		pixel_x2 = static_cast<int>(floor((clip_x2 - origin_x) / scale_x));
		pixel_y2 = static_cast<int>(floor((clip_y2 - origin_y) / scale_y));

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

    // load raster from gdal if (some) requested pixels are inside raster
    bool loadGdal = overlaps(pixel_x1, pixel_x2, 0, nXSize - 1) && overlaps(pixel_y1, pixel_y2, 0, nYSize - 1);
    if (loadGdal) {
        // limit to (0, nXSize), (0, nySize) for gdal
        int gdal_pixel_x1 = std::min(nXSize - 1, std::max(0, pixel_x1));
        int gdal_pixel_y1 = std::min(nYSize - 1, std::max(0, pixel_y1));

        int gdal_pixel_x2 = std::min(nXSize - 1, std::max(0, pixel_x2));
        int gdal_pixel_y2 = std::min(nYSize - 1, std::max(0, pixel_y2));

        int gdal_pixel_width = gdal_pixel_x2 - gdal_pixel_x1 + 1;
        int gdal_pixel_height = gdal_pixel_y2 - gdal_pixel_y1 + 1;

        // compute the rectangle for the gdal raster
        double gdal_x1 = origin_x + scale_x * gdal_pixel_x1;
        double gdal_y1 = origin_y + scale_y * gdal_pixel_y1;
        double gdal_x2 = gdal_x1 + scale_x * gdal_pixel_width;
        double gdal_y2 = gdal_y1 + scale_y * gdal_pixel_height;

        // query scale and factor
        double query_scale_x = (qrect.x2 - qrect.x1) / qrect.xres;
        double query_scale_y = (qrect.y2 - qrect.y1) / qrect.yres;

        double query_scale_factor_x = std::abs(scale_x / query_scale_x);
        double query_scale_factor_y = std::abs(scale_y / query_scale_y);


        std::unique_ptr<GenericRaster> raster;
        SpatioTemporalReference stref(
                SpatialReference(qrect.epsg, gdal_x1, gdal_y1, gdal_x2, gdal_y2, flipx, flipy),
                TemporalReference(tref.timetype, tref.t1, tref.t2)
        );

        DataDescription dd(type, loadingInfo.unit, hasnodata, nodata);

        // read the actual data
        int gdal_raster_width = static_cast<int> (std::ceil(gdal_pixel_width * query_scale_factor_x));
        int gdal_raster_height = static_cast<int> (std::ceil(gdal_pixel_height * query_scale_factor_y));
        raster = GenericRaster::create(dd, stref, static_cast<uint32_t>(gdal_raster_width),
                                            static_cast<uint32_t>(gdal_raster_height));
        void *buffer = raster->getDataForWriting();

        auto res = poBand->RasterIO(GF_Read,
                                    gdal_pixel_x1, gdal_pixel_y1, gdal_pixel_width,
                                    gdal_pixel_height,  // rectangle in the source raster
                                    buffer, raster->width,
                                    raster->height,  // position and size of the destination buffer
                                    type, 0, 0, nullptr);

        if (res != CE_None) {
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

            DataDescription dd(type, loadingInfo.unit, hasnodata, nodata);

            auto raster2 = GenericRaster::create(dd, stref, qrect.xres, qrect.yres);

            if (raster) {
                int gdal_pixel_offset_x = gdal_pixel_x1 - pixel_x1;
                int gdal_pixel_offset_y = gdal_pixel_y1 - pixel_y1;
                int blit_offset_x = static_cast<int>(gdal_pixel_offset_x * query_scale_factor_x);
                int blit_offset_y = static_cast<int>(gdal_pixel_offset_y * query_scale_factor_y);
                raster2->blit(raster.get(), blit_offset_x, blit_offset_y);
            }

            return raster2;
        } else {
			return raster;
		}

    } else {
        // return empty raster
        SpatioTemporalReference stref(
                SpatialReference(qrect.epsg, x1, y1, x2, y2, flipx, flipy),
                TemporalReference(tref.timetype, tref.t1, tref.t2)
        );

        DataDescription dd(type, loadingInfo.unit, hasnodata, nodata);

        return GenericRaster::create(dd, stref, qrect.xres, qrect.yres);
    }

	//GDALRasterBand is not to be freed, is owned by GDALDataset that will be closed later
}

//load the GDALDataset from disk
std::unique_ptr<GenericRaster> RasterGDALSourceOperator::loadDataset(const GDALTimesnap::GDALDataLoadingInfo &loadingInfo,
                                                                     epsg_t epsg, bool clip, const QueryRectangle &qrect) {
	
	GDAL::init();

	GDALDataset *dataset = (GDALDataset *) GDALOpen(loadingInfo.fileName.c_str(), GA_ReadOnly);

	if (dataset == NULL)
		throw OperatorException(concat("GDAL Source: Could not open dataset ", loadingInfo.fileName));

	//read GeoTransform to get origin and scale
	double adfGeoTransform[6];
	if( dataset->GetGeoTransform( adfGeoTransform ) != CE_None ) {
		GDALClose(dataset);
		throw OperatorException("GDAL Source: No GeoTransform information in raster");
	}

	int rastercount = dataset->GetRasterCount();
	if (loadingInfo.channel < 1 || loadingInfo.channel > rastercount) {
		GDALClose(dataset);
		throw OperatorException("GDAL Source: rasterid not found");
	}

	auto raster = loadRaster(dataset, loadingInfo.channel, adfGeoTransform[0], adfGeoTransform[3], adfGeoTransform[1],
                             adfGeoTransform[5], epsg, clip, qrect.x1, qrect.y1, qrect.x2, qrect.y2, qrect, loadingInfo.tref, loadingInfo);

	GDALClose(dataset);

	return raster;
}


