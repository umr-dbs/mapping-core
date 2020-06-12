#include "datatypes/raster.h"
#include "rasterdb/rasterdb.h"
#include "raster/opencl.h"
#include "operators/operator.h"
#include "util/gdal_timesnap.h"
#include "util/gdal.h"
#include "util/gdal_source_datasets.h"
#include "util/gdal_dataset_importer.h"
#include "util/configuration.h"
#include "util/log.h"

/**
 * Operator that loads raster data via gdal. Loads them from imported GDAL dataset, import via GDAL dataset importer.
 * Parameters:
 * 		sourcename: the name of the imported gdal dataset.
 *		channel:	which channel is to be loaded (channels are 1 based)
 */
class RasterGDALSourceOperator : public GenericOperator {
	public:
        /**
         * Generic operator constructor
         */
		RasterGDALSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);
		~RasterGDALSourceOperator() override;

        /**
         * Writes the provenance information to a `ProvenanceCollection`
         */
		void getProvenance(ProvenanceCollection &pc) override;

		/**
		 * This operator only returns raster images
		 */
		std::unique_ptr<GenericRaster> getRaster(const QueryRectangle &rect, const QueryTools &tools) override;

	protected:
		void writeSemanticParameters(std::ostringstream &stream) override;

	private:
		std::string sourcename;
		int channel;

		Json::Value gdalParams;

		std::unique_ptr<GenericRaster> loadDataset( const GDALTimesnap::GDALDataLoadingInfo &loadingInfo,
													CrsId crsId,
													bool clip,
													const QueryRectangle &qrect,
													const QueryTools &tools);

		std::unique_ptr<GenericRaster> loadRaster(  GDALDataset *dataset, double origin_x, double origin_y,
													double scale_x, double scale_y,
													CrsId crsId, bool clip,
													double clip_x1, double clip_y1,
													double clip_x2, double clip_y2,
													const QueryRectangle &qrect,
                                                    const GDALTimesnap::GDALDataLoadingInfo &loadingInfo);
};


RasterGDALSourceOperator::RasterGDALSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params) : GenericOperator(sourcecounts, sources) {
	//assumeSources(0);
	sourcename = params.get("sourcename", "").asString();
	if (sourcename.length() == 0)
		throw OperatorException("SourceOperator: missing sourcename");
	channel = params.get("channel", 1).asInt();

	gdalParams = params.get("gdal_params", Json::objectValue);
}

RasterGDALSourceOperator::~RasterGDALSourceOperator() = default;

REGISTER_OPERATOR(RasterGDALSourceOperator, "gdal_source");

void RasterGDALSourceOperator::getProvenance(ProvenanceCollection &pc) {
    std::string local_identifier;
    Json::Value datasetJson;

    if (gdalParams.isMember("channels")) {
        local_identifier = "data.gdal_source." + gdalParams["channels"][channel]["file_name"].asString();
        datasetJson = gdalParams;
    } else {
        local_identifier = "data.gdal_source." + sourcename;
        datasetJson = GDALSourceDataSets::getDataSetDescription(sourcename);
    }

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
	Json::Value params;

	params["sourcename"] = sourcename;
	params["channel"] = channel;

	params["gdal_params"] = gdalParams;

	Json::FastWriter writer;
	stream << writer.write(params);
}

// load the json definition of the dataset, then get the file to be loaded from GDALTimesnap. Finally load the raster.
std::unique_ptr<GenericRaster> RasterGDALSourceOperator::getRaster(const QueryRectangle &rect, const QueryTools &tools) {
    Json::Value datasetJson;

    if (gdalParams.isMember("channels")) {
        datasetJson = gdalParams;
        Log::debug("getRaster: Using gdalParams.");
    } else {
        datasetJson = GDALSourceDataSets::getDataSetDescription(sourcename);
        Log::debug("getRaster: Using getDataSetDescription.");
    }

	GDALTimesnap::GDALDataLoadingInfo loadingInfo = GDALTimesnap::getDataLoadingInfo(datasetJson, channel, rect);
	auto raster = loadDataset(loadingInfo, rect.crsId, true, rect, tools);
	//flip here so the tiff result will not be flipped
	return raster->flip(false, true);
}

bool overlaps (double a_start, double a_end, double b_start, double b_end) {
	return a_end >= a_start && b_end >= b_start && a_end >= b_start && a_start <= b_end;
}

// loads the raster and read the wanted raster data section into a GenericRaster
std::unique_ptr<GenericRaster> RasterGDALSourceOperator::loadRaster(GDALDataset *dataset, double origin_x,
																	double origin_y, double scale_x, double scale_y,
																	CrsId crsId, bool clip, double clip_x1,
																	double clip_y1, double clip_x2, double clip_y2,
																	const QueryRectangle& qrect,
																	const GDALTimesnap::GDALDataLoadingInfo &loadingInfo) {
	// get raster metadata
    GDALRasterBand  *poBand;
	int             nBlockXSize, nBlockYSize;

	bool flipx = false, flipy = false;

	poBand = dataset->GetRasterBand( loadingInfo.channel );
	poBand->GetBlockSize( &nBlockXSize, &nBlockYSize );

	GDALDataType type = poBand->GetRasterDataType();

    bool hasnodata;
    double nodata;

    if (std::isnan(loadingInfo.nodata)) {
        int success;
        nodata = poBand->GetNoDataValue(&success);

		hasnodata = success != 0;
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
        SpatioTemporalReference stref_gdal(
                SpatialReference(qrect.crsId, gdal_x1, gdal_y1, gdal_x2, gdal_y2, flipx, flipy),
                loadingInfo.tref
        );

        DataDescription dd(type, loadingInfo.unit, hasnodata, nodata);

        // read the actual data
		auto gdal_raster_width = static_cast<int> (std::ceil(gdal_pixel_width * query_scale_factor_x));
		auto gdal_raster_height = static_cast<int> (std::ceil(gdal_pixel_height * query_scale_factor_y));
        raster = GenericRaster::create(dd, stref_gdal, static_cast<uint32_t>(gdal_raster_width),
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
                    SpatialReference(qrect.crsId, x1, y1, x2, y2, flipx, flipy),
                    loadingInfo.tref
            );

            auto raster2 = GenericRaster::create(dd, stref, qrect.xres, qrect.yres);
            if(hasnodata && nodata != 0) {
                raster2->clear(nodata);
            }

            if (raster) {
                int gdal_pixel_offset_x = gdal_pixel_x1 - pixel_x1;
                int gdal_pixel_offset_y = gdal_pixel_y1 - pixel_y1;
				auto blit_offset_x = static_cast<int>(gdal_pixel_offset_x * query_scale_factor_x);
				auto blit_offset_y = static_cast<int>(gdal_pixel_offset_y * query_scale_factor_y);
                raster2->blit(raster.get(), blit_offset_x, blit_offset_y);
            }

            return raster2;
        } else {
			return raster;
		}

    } else {
        // return empty raster
        SpatioTemporalReference stref(
                SpatialReference(qrect.crsId, x1, y1, x2, y2, flipx, flipy),
                loadingInfo.tref
        );

        DataDescription dd(type, loadingInfo.unit, hasnodata, nodata);

        auto raster = GenericRaster::create(dd, stref, qrect.xres, qrect.yres);
        if(hasnodata && nodata != 0) {
            raster->clear(nodata);
        }
        return raster;
    }

	//GDALRasterBand is not to be freed, is owned by GDALDataset that will be closed later
}

void injectParameters(std::string &file, const QueryRectangle &qrect, const QueryTools &tools) {
    boost::replace_all(file, "%%%MINX%%%", std::to_string(qrect.x1));
    boost::replace_all(file, "%%%MINY%%%", std::to_string(qrect.y1));
    boost::replace_all(file, "%%%MAXX%%%", std::to_string(qrect.x2));
    boost::replace_all(file, "%%%MAXY%%%", std::to_string(qrect.y2));

    boost::replace_all(file, "%%%XRES%%%", std::to_string(qrect.xres));
    boost::replace_all(file, "%%%YRES%%%", std::to_string(qrect.yres));

    boost::replace_all(file, "%%%T1%%%", std::to_string(qrect.t1));
    boost::replace_all(file, "%%%T2%%%", std::to_string(qrect.t2));

    UserDB::User &user = tools.session->getUser();
    for (auto& key : Configuration::getVector<std::string>("gdal_source.injectable_user_artifacts")) {
        size_t split = key.find(':');
        if (split) {
            std::string type = key.substr(0, split);
            std::string name = key.substr(split + 1);
            std::string placeholder = "%%%" + key + "%%%";

            try {
                boost::replace_all(file, placeholder, user.loadArtifact(user.getUsername(), type,
                                                                        name)->getLatestArtifactVersion()->getValue());
            } catch(UserDB::artifact_error&) {}
        }
    }
}

//load the GDALDataset from disk
std::unique_ptr<GenericRaster> RasterGDALSourceOperator::loadDataset(const GDALTimesnap::GDALDataLoadingInfo &loadingInfo,
                                                                     CrsId crsId, bool clip, const QueryRectangle &qrect,
                                                                     const QueryTools &tools) {
	GDAL::init();
	std::string fileName = loadingInfo.fileName;
    injectParameters(fileName, qrect, tools);
    Log::debug(concat("loadDataset: using filename: ", fileName.c_str()));

	auto dataset = (GDALDataset *) GDALOpen(fileName.c_str(), GA_ReadOnly);

	if (dataset == nullptr)
		throw OperatorException(concat("GDAL Source: Could not open dataset ", loadingInfo.fileName));

	if (crsId != loadingInfo.crsId) {
		throw OperatorException("GDAL Source: Requested wrong CrsId");
	}

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

	auto raster = loadRaster(dataset, adfGeoTransform[0], adfGeoTransform[3], adfGeoTransform[1],
                             adfGeoTransform[5], crsId, clip, qrect.x1, qrect.y1, qrect.x2, qrect.y2, qrect, loadingInfo);

	GDALClose(dataset);

	return raster;
}


