
#include "datatypes/raster.h"
#include "datatypes/raster/typejuggling.h"
#include "rasterdb/rasterdb.h"
#include "rasterdb/backend.h"
#include "rasterdb/backend_local.h"
#include "rasterdb/backend_remote.h"
#include "converters/converter.h"
#include "util/sqlite.h"
#include "util/configuration.h"
#include "operators/operator.h"


#include <unordered_map>
#include <mutex>
#include <limits.h>
#include <stdio.h>
#include <cstdlib>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <iostream>
#include <fstream>
#include <memory>
#include <string>

#include <json/json.h>


const int DEFAULT_TILE_SIZE = 1024;


bool GDALCRS::operator==(const GDALCRS &b) const {
	if (dimensions != b.dimensions)
		return false;
	for (int i=0;i<dimensions;i++) {
		if (size[i] != b.size[i]) {
			std::cerr << "size mismatch" << std::endl;
			return false;
		}
		if (fabs(origin[i] - b.origin[i]) > 0.5) {
			std::cerr << "origin mismatch: " << fabs(origin[i] - b.origin[i]) << std::endl;
			return false;
		}
		if (fabs(scale[i] / b.scale[i] - 1.0) > 0.001) {
			std::cerr << "scale mismatch" << std::endl;
			return false;
		}
	}
	return true;
}

void GDALCRS::verify() const {
	if (dimensions < 1 || dimensions > 3)
		throw MetadataException("Amount of dimensions not between 1 and 3");
	for (int i=0;i<dimensions;i++) {
		if (/*size[i] < 0 || */ size[i] > 1<<24)
			throw MetadataException("Size out of limits");
		if (scale[i] == 0)
			throw MetadataException("Scale cannot be 0");
	}
}

size_t GDALCRS::getPixelCount() const {
	if (dimensions == 1)
		return (size_t) size[0];
	if (dimensions == 2)
		return (size_t) size[0] * size[1];
	if (dimensions == 3)
		return (size_t) size[0] * size[1] * size[2];
	throw MetadataException("Amount of dimensions not between 1 and 3");
}

SpatioTemporalReference GDALCRS::toSpatioTemporalReference(bool &flipx, bool &flipy, timetype_t timetype, double t1, double t2) const {
	double x1 = origin[0];
	double y1 = origin[1];
	double x2 = origin[0] + scale[0] * size[0];
	double y2 = origin[1] + scale[1] * size[1];

	return SpatioTemporalReference(epsg, x1, y1, x2, y2, flipx, flipy, timetype, t1, t2);
}

std::ostream& operator<< (std::ostream &out, const GDALCRS &rm) {
	out << "GDALCRS(epsg=" << (int) rm.epsg << " dim=" << rm.dimensions << " size=["<<rm.size[0]<<","<<rm.size[1]<<"] origin=["<<rm.origin[0]<<","<<rm.origin[1]<<"] scale=["<<rm.scale[0]<<","<<rm.scale[1]<<"])";
	return out;
}



class RasterDBChannel {
	public:
		//friend class RasterDB;

		RasterDBChannel(const DataDescription &dd) : dd(dd), has_transform(false) {}
		~RasterDBChannel() {}

		void setTransform(GDALDataType datatype, double offset, double scale, const std::string &offset_metadata, const std::string &scale_metadata) {
			has_transform = true;
			transform_offset = offset;
			transform_scale = scale;
			transform_offset_metadata = offset_metadata;
			transform_scale_metadata = scale_metadata;
			transform_datatype = datatype == GDT_Unknown ? dd.datatype : datatype;
		}
		double getOffset(const DirectMetadata<double> &md) {
			if (!has_transform)
				return 0;
			if (transform_offset_metadata.length() > 0)
				return md.get(transform_offset_metadata, 0.0);
			return transform_offset;
		}
		double getScale(const DirectMetadata<double> &md) {
			if (!has_transform)
				return 0;
			if (transform_scale_metadata.length() > 0)
				return md.get(transform_scale_metadata, 1.0);
			return transform_scale;
		}
		DataDescription getTransformedDD(const DirectMetadata<double> &md) {
			if (!has_transform)
				return dd;
			double offset = getOffset(md);
			double scale = getScale(md);
			double transformed_min = dd.min * scale + offset;
			double transformed_max = dd.max * scale + offset;
			DataDescription transformed_dd(transform_datatype, transformed_min, transformed_max);
			transformed_dd.addNoData();
			transformed_dd.verify();
			return transformed_dd;
		}

		bool hasTransform() { return has_transform; }

		const DataDescription dd;
	private:
		bool has_transform;
		GDALDataType transform_datatype;
		double transform_offset;
		double transform_scale;
		std::string transform_offset_metadata;
		std::string transform_scale_metadata;

};



RasterDB::RasterDB(const char *sourcename, bool writeable)
	: writeable(writeable), crs(nullptr), channelcount(0), channels(nullptr) {
	try {
		auto backendtype = Configuration::get("rasterdb.backend", "local");

		if (backendtype == "remote")
			backend.reset( new RemoteRasterDBBackend(sourcename, writeable) );
		else
			backend.reset( new LocalRasterDBBackend(sourcename, writeable) );

		init();
	}
	catch (const std::exception &e) {
		cleanup();
		throw;
	}
}

RasterDB::~RasterDB() {
	cleanup();
}


void RasterDB::init() {
	/*
	 * Step #1: parse the json
	 */
	auto json = backend->readJSON();
	Json::Reader reader(Json::Features::strictMode());
	Json::Value root;
	if (!reader.parse(json, root)) {
		//printf("unable to read json\n%s\n", reader.getFormattedErrorMessages().c_str());
		throw SourceException("unable to parse json");
	}

	Json::Value jrm = root["coords"];
	Json::Value sizes = jrm["size"];
	Json::Value origins = jrm["origin"];
	Json::Value scales = jrm["scale"];
	int dimensions = sizes.size();
	if (dimensions != (int) origins.size() || dimensions != (int) scales.size())
		throw SourceException("json invalid, different dimensions in data");
	epsg_t epsg = (epsg_t) jrm.get("epsg", EPSG_UNKNOWN).asInt();

	if (dimensions == 2) {
		crs = new GDALCRS(
			epsg,
			sizes.get((Json::Value::ArrayIndex) 0, -1).asInt(), sizes.get((Json::Value::ArrayIndex) 1, -1).asInt(),
			origins.get((Json::Value::ArrayIndex) 0, 0).asDouble(), origins.get((Json::Value::ArrayIndex) 1, 0).asDouble(),
			scales.get((Json::Value::ArrayIndex) 0, 0).asDouble(), scales.get((Json::Value::ArrayIndex) 1, 0).asDouble()
		);
	}
	else
		throw SourceException("json invalid, can only process two-dimensional rasters");

	crs->verify();

	Json::Value channelinfo = root["channels"];
	if (!channelinfo.isArray() || channelinfo.size() < 1) {
		throw SourceException("No channel information in json");
	}

	channelcount = channelinfo.size();
	channels = new RasterDBChannel *[channelcount];
	for (int i=0;i<channelcount;i++)
		channels[i] = nullptr;

	for (int i=0;i<channelcount;i++) {
		Json::Value channel = channelinfo[(Json::Value::ArrayIndex) i];

		std::string datatype = channel.get("datatype", "unknown").asString();
		bool has_no_data = false;
		double no_data = 0;
		if (channel.isMember("nodata")) {
			has_no_data = true;
			no_data = channel.get("nodata", 0).asDouble();
		}

		channels[i] = new RasterDBChannel(DataDescription(
			GDALGetDataTypeByName(datatype.c_str()),
			channel.get("min", 0).asDouble(),
			channel.get("max", -1).asDouble(),
			has_no_data, no_data
		));
		if (channel.isMember("transform")) {
			Json::Value transform = channel["transform"];
			Json::Value offset = transform["offset"];
			Json::Value scale = transform["scale"];
			channels[i]->setTransform(
				GDALGetDataTypeByName(transform.get("datatype", "unknown").asString().c_str()),
				offset.type() != Json::stringValue ? offset.asDouble() : 0.0,
				scale.type()  != Json::stringValue ? scale.asDouble()  : 0.0,
				offset.type() == Json::stringValue ? offset.asString() : "",
				scale.type()  == Json::stringValue ? scale.asString()  : ""
			);

		}
		channels[i]->dd.verify();
	}
}


void RasterDB::cleanup() {
	if (crs) {
		delete crs;
		crs = nullptr;
	}
	if (channelcount && channels) {
		for (int i=0;i<channelcount;i++) {
			delete channels[i];
		}
		delete [] channels;
	}
}



void RasterDB::import(const char *filename, int sourcechannel, int channelid, double time_start, double time_end, RasterConverter::Compression compression) {
	if (!isWriteable())
		throw SourceException("Cannot import into a source opened as read-only");
	bool raster_flipx, raster_flipy;
	auto raster = GenericRaster::fromGDAL(filename, sourcechannel, raster_flipx, raster_flipy, crs->epsg);

	bool crs_flipx, crs_flipy;
	SpatioTemporalReference stref(crs->epsg, crs->origin[0], crs->origin[1], crs->origin[0]+crs->scale[0], crs->origin[1]+crs->scale[1], crs_flipx, crs_flipy, TIMETYPE_UNREFERENCED, 0, 1);

	bool need_flipx = raster_flipx != crs_flipx;
	bool need_flipy = raster_flipy != crs_flipy;

	//printf("GDAL: %d %d\nCRS:  %d %d\nflip: %d %d\n", raster_flipx, raster_flipy, crs_flipx, crs_flipy, need_flipx, need_flipy);

	if (need_flipx || need_flipy) {
		raster = raster->flip(need_flipx, need_flipy);
	}

	import(raster.get(), channelid, time_start, time_end, compression);
}


void RasterDB::import(GenericRaster *raster, int channelid, double time_start, double time_end, RasterConverter::Compression compression) {
	if (!isWriteable())
		throw SourceException("Cannot import into a source opened as read-only");
	if (channelid < 0 || channelid >= channelcount)
		throw SourceException("RasterDB::import: unknown channel");
	/*
	if (!(raster->lcrs == *lcrs)) {
		std::cerr << "Imported CRS: " << raster->lcrs << std::endl;
		std::cerr << "Expected CRS: " << *lcrs << std::endl;
		throw SourceException("Local CRS does not match RasterDB");
	}
	*/

	// If the no_data value is missing in the import raster, we assume this to be a GDAL error.
	// In this case, we add the no_data value and continue as planned.
	DataDescription rasterdd = raster->dd;
	if (channels[channelid]->dd.has_no_data && !rasterdd.has_no_data) {
		rasterdd.has_no_data = true;
		rasterdd.no_data = channels[channelid]->dd.no_data;
	}
/*
	if (!(rasterdd == *channels[channelid])) {
		std::cerr << "imported raster: " << raster->dd << "expected:        " << *(channels[channelid]);
		throw SourceException("DataDescription does not match Channel's DataDescription");
	}
*/
	uint32_t tilesize = DEFAULT_TILE_SIZE;

	printf("starting import for raster of size %d x %d, time %f -> %f\n", raster->width, raster->height, time_start, time_end);

	auto rasterid = backend->createRaster(channelid, time_start, time_end, raster->md_string, raster->md_value);

	for (int zoom=0;;zoom++) {
		int zoomfactor = 1 << zoom;

		if (zoom > 0 && crs->size[0] / zoomfactor < tilesize && crs->size[1] / zoomfactor < tilesize && crs->size[2] / zoomfactor < tilesize)
			break;

		GenericRaster *zoomedraster = raster;
		std::unique_ptr<GenericRaster> zoomedraster_guard;
		if (zoom > 0) {
			printf("  Scaling for zoom %d to %u x %u x %u pixels\n", zoom, crs->size[0] / zoomfactor, crs->size[1] / zoomfactor, crs->size[2] / zoomfactor);
			zoomedraster_guard = raster->scale(crs->size[0] / zoomfactor, crs->size[1] / zoomfactor, crs->size[2] / zoomfactor);
			zoomedraster = zoomedraster_guard.get();
			printf("  done scaling\n");
		}

		/*for (uint32_t zoff = 0; zoff == 0 || zoff < zoomedraster->lcrs.size[2]; zoff += tilesize)*/ {
			uint32_t zoff = 0;
			uint32_t zsize = 0; //std::min(zoomedraster->lcrs.size[2] - zoff, tilesize);
			for (uint32_t yoff = 0; yoff == 0 || yoff < zoomedraster->height; yoff += tilesize) {
				uint32_t ysize = std::min(zoomedraster->height - yoff, tilesize);
				for (uint32_t xoff = 0; xoff < zoomedraster->width; xoff += tilesize) {
					uint32_t xsize = std::min(zoomedraster->width - xoff, tilesize);

					printf("    importing tile at zoom %d with size %u: (%u, %u, %u) at offset (%u, %u, %u)\n", zoom, tilesize, xsize, ysize, zsize, xoff, yoff, zoff);
					if (backend->hasTile(rasterid, xsize, ysize, zsize, xoff*zoomfactor, yoff*zoomfactor, zoff*zoomfactor, zoom)) {
						printf("      skipping..\n");
						continue;
					}

					auto tile = GenericRaster::create(channels[channelid]->dd, SpatioTemporalReference::unreferenced(), xsize, ysize, zsize);
					tile->blit(zoomedraster, -(int)xoff, -(int)yoff, -(int)zoff);

					auto buffer = RasterConverter::direct_encode(tile.get(), compression);

					backend->writeTile(rasterid, *buffer, xsize, ysize, zsize, xoff*zoomfactor, yoff*zoomfactor, zoff*zoomfactor, zoom, compression);
					printf("    tile saved, compression %d, size: %ld -> %ld (%f)\n", (int) compression, tile->getDataSize(), buffer->size, (double) buffer->size / tile->getDataSize());
				}
			}
		}
	}
}


void RasterDB::linkRaster(int channelid, double time_of_reference, double time_start, double time_end) {
	if (!isWriteable())
		throw SourceException("Cannot link rasters in a source opened as read-only");

	backend->linkRaster(channelid, time_of_reference, time_start, time_end);
}


template<typename T1, typename T2>
struct raster_transformed_blit {
	static void execute(Raster2D<T1> *raster_dest, Raster2D<T2> *raster_src, int destx, int desty, int destz, double offset, double scale) {
		int x1 = std::max(destx, 0);
		int y1 = std::max(desty, 0);
		int x2 = std::min(raster_dest->width, destx+raster_src->width);
		int y2 = std::min(raster_dest->height, desty+raster_src->height);

		if (x1 >= x2 || y1 >= y2)
			throw ArgumentException(concat("transformedBlit without overlapping region: ", raster_src->width, "x", raster_src->height, " blitted onto ", raster_dest->width, "x", raster_dest->height, " at (", destx, ",", desty, "), overlap (", x1, ",", y1, ") -> (", x2, ",", y2, ")"));

		for (int y=y1;y<y2;y++) {
			for (int x=x1;x<x2;x++) {
				T2 val = raster_src->get(x-destx, y-desty);
				if (raster_src->dd.is_no_data(val))
					raster_dest->set(x, y, raster_dest->dd.no_data);
				else
					raster_dest->set(x, y, ((T1) val) * scale + offset);
			}
		}
	}
};


static void transformedBlit(GenericRaster *dest, GenericRaster *src, int destx, int desty, int destz, double offset, double scale) {
	if (src->getRepresentation() != GenericRaster::Representation::CPU || dest->getRepresentation() != GenericRaster::Representation::CPU)
		throw MetadataException("transformedBlit from raster that's not in a CPU buffer");

	callBinaryOperatorFunc<raster_transformed_blit>(dest, src, destx, desty, destz, offset, scale);
}

std::unique_ptr<GenericRaster> RasterDB::load(int channelid, double timestamp, int x1, int y1, int x2, int y2, int zoom, bool transform, size_t *io_cost) {
	if (io_cost)
		*io_cost = 0;

	if (channelid < 0 || channelid >= channelcount)
		throw SourceException("RasterDB::load: unknown channel");

	auto rasterdescription = backend->getClosestRaster(channelid, timestamp);
	auto rasterid = rasterdescription.rasterid;
	zoom = backend->getBestZoom(rasterid, zoom);
	int zoomfactor = 1 << zoom;

	// Figure out the CRS after cutting and zooming
	auto width = (x2-x1) >> zoom;
	auto height = (y2-y1) >> zoom;
	auto scale_x = crs->scale[0]*zoomfactor;
	auto scale_y = crs->scale[1]*zoomfactor;
	auto origin_x = crs->PixelToWorldX(x1);
	auto origin_y = crs->PixelToWorldY(y1);
	GDALCRS zoomed_and_cut_crs(crs->epsg, width, height, origin_x, origin_y, scale_x, scale_y);

	bool flipx, flipy;
	auto resultstref = zoomed_and_cut_crs.toSpatioTemporalReference(flipx, flipy, TIMETYPE_UNIX, rasterdescription.time_start, rasterdescription.time_end);

	/*
	if (x2 != x1 + (width << zoom) || y2 != y1 + (height << zoom))
		throw SourceException(concat("RasterDB::load, fractions of a pixel requested: (x: ", x2, " <-> ", (x1 + (width<<zoom)), " y: ", y2, " <-> ", (y1 + (height<<zoom))));
	*/
	// Make sure no fractional pixels are requested
	//x2 = x1 + (width << zoom);
	//y2 = y1 + (height << zoom);

	if (x1 > x2 || y1 > y2)
		throw SourceException(concat("RasterDB::load(", channelid, ", ", timestamp, ", [",x1,",",y1," -> ",x2,",",y2,"]): coords swapped"));

	decltype(GenericRaster::md_value) result_md_value;
	decltype(GenericRaster::md_string) result_md_string;
	backend->readAttributes(rasterid, result_md_string, result_md_value);

	DataDescription transformed_dd = transform ? channels[channelid]->getTransformedDD(result_md_value) : channels[channelid]->dd;
	auto result = GenericRaster::create(transformed_dd, resultstref, width, height);
	result->clear(transformed_dd.no_data);

	// Load all overlapping parts and blit them onto the empty raster
	auto tiles = backend->enumerateTiles(channelid, rasterid, x1, y1, x2, y2, zoom);
	if (tiles.size() <= 0)
		throw SourceException("RasterDB::load(): No matching tiles found in DB");

	for (auto &tile : tiles) {
		auto tile_buffer = backend->readTile(tile);

		auto tile_raster = RasterConverter::direct_decode(*tile_buffer, channels[channelid]->dd, SpatioTemporalReference::unreferenced(), tile.width, tile.height, tile.depth, tile.compression);
		if (io_cost)
			*io_cost += tile.size;

		if (transform && channels[channelid]->hasTransform()) {
			transformedBlit(
				result.get(), tile_raster.get(),
				((int64_t) tile.x1-x1) >> zoom, ((int64_t) tile.y1-y1) >> zoom, 0/* (r_z1-z1) >> zoom*/,
				channels[channelid]->getOffset(result_md_value), channels[channelid]->getScale(result_md_value));
		}
		else
			result->blit(tile_raster.get(), ((int64_t) tile.x1-x1) >> zoom, ((int64_t) tile.y1-y1) >> zoom, 0/* ((int64_t) r_z1-z1) >> zoom*/);
	}

	if (flipx || flipy) {
		result = result->flip(flipx, flipy);
	}

	result->md_value = std::move(result_md_value);
	result->md_string = std::move(result_md_string);
	result->md_value.set("Channel", channelid);
	return result;
}


std::unique_ptr<GenericRaster> RasterDB::query(const QueryRectangle &rect, QueryProfiler &profiler, int channelid, bool transform) {
	if (crs->epsg != rect.epsg)
		throw OperatorException(concat("SourceOperator: wrong epsg requested. Source is ", (int) crs->epsg, ", requested ", (int) rect.epsg));

	// Get all pixel coordinates that need to be returned. The endpoints of the QueryRectangle are inclusive.
	double px1 = crs->WorldToPixelX(rect.x1);
	double py1 = crs->WorldToPixelY(rect.y1);
	double px2 = crs->WorldToPixelX(rect.x2);
	double py2 = crs->WorldToPixelY(rect.y2);

	// All Pixels even partially inside the rectangle need to be returned.
	int pixel_x1 = std::floor(std::min(px1,px2));
	int pixel_y1 = std::floor(std::min(py1,py2));
	int pixel_x2 = std::ceil(std::max(px1,px2))+1; // +1 because x2/y2 are not inclusive
	int pixel_y2 = std::ceil(std::max(py1,py2))+1;

	// Figure out the desired zoom level
	int zoom = 0;
	uint32_t pixel_width = pixel_x2 - pixel_x1;
	uint32_t pixel_height = pixel_y2 - pixel_y1;
	while (pixel_width > 2*rect.xres && pixel_height > 2*rect.yres) {
		zoom++;
		pixel_width >>= 1;
		pixel_height >>= 1;
	}

	size_t io_costs = 0;
	auto result = load(channelid, rect.t1, pixel_x1, pixel_y1, pixel_x2, pixel_y2, zoom, transform, &io_costs);
	// TODO: automatically fail if rect.t2 is outside of the result's area?
	profiler.addIOCost(io_costs);
	return result;
}




static std::unordered_map<std::string, std::weak_ptr<RasterDB> > RasterDB_map;
static std::mutex RasterDB_map_mutex;

std::shared_ptr<RasterDB> RasterDB::open(const char *sourcename, bool writeable) {
	std::lock_guard<std::mutex> guard(RasterDB_map_mutex);

	std::string name(sourcename);

	if (RasterDB_map.count(name) == 1) {
		auto &weak_ptr = RasterDB_map.at(name);
		auto shared_ptr = weak_ptr.lock();
		if (shared_ptr) {
			if (writeable && !shared_ptr->isWriteable())
				throw new SourceException("Cannot re-open source as read/write (TODO?)");
			return shared_ptr;
		}
		RasterDB_map.erase(name);
	}

	auto shared_ptr = std::make_shared<RasterDB>(name.c_str(), writeable);
	RasterDB_map[name] = std::weak_ptr<RasterDB>(shared_ptr);

	if (writeable && !shared_ptr->isWriteable())
		throw new SourceException("Cannot re-open source as read/write (TODO?)");
	return shared_ptr;
}

