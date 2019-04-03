
#include "datatypes/raster.h"
#include "datatypes/raster/typejuggling.h"
#include "datatypes/simplefeaturecollections/geosgeomutil.h"
#include "operators/operator.h"
#include "util/gdal.h"
#include "datatypes/pointcollection.h"
#include "datatypes/polygoncollection.h"
#include "datatypes/raster_time_series.h"
#include "operators/raster_time_series/raster_time_series_operator.h"
#include "raster_time_series/operator_state/projection_state.h"
#include "raster_time_series/raster_calculations.h"

#include <memory>
#include <sstream>
#include <cmath>
#include <vector>
#include <json/json.h>
#include <geos/geom/util/GeometryTransformer.h>

/**
 * Operator that projects raster and feature data to a given projection
 *
 * Parameters:
 * - src_crsId: the crsId of the source projection
 * - dest_crsId: the crsId of the destination projection
 */
class ProjectionOperator : public rts::RasterTimeSeriesOperator {
	public:
		ProjectionOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);
		~ProjectionOperator() override;

#ifndef MAPPING_OPERATOR_STUBS
		std::unique_ptr<GenericRaster> getRaster(const QueryRectangle &rect, const QueryTools &tools) override;
		std::unique_ptr<PointCollection> getPointCollection(const QueryRectangle &rect, const QueryTools &tools) override;
		std::unique_ptr<LineCollection> getLineCollection(const QueryRectangle &rect, const QueryTools &tools) override;
		std::unique_ptr<PolygonCollection> getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) override;
        std::unique_ptr<rts::RasterTimeSeries> getRasterTimeSeries(const QueryRectangle &rect, const QueryTools &tools, rts::ProcessingOrder processingOrder) override;
#endif
	protected:
		void writeSemanticParameters(std::ostringstream &stream) override;
        bool supportsProcessingOrder(rts::ProcessingOrder processingOrder) const override;
		rts::OptionalDescriptor nextDescriptor(rts::OperatorState &state, const QueryRectangle &rect, const QueryTools &tools) override;
private:
		QueryRectangle projectQueryRectangle(const QueryRectangle &rect, const GDAL::CRSTransformer &transformer);
		CrsId src_crsId, dest_crsId;
};


#if 0
class MeteosatLatLongOperator : public GenericOperator {
	public:
	MeteosatLatLongOperator(int sourcecount, GenericOperator *sources[]);
		virtual ~MeteosatLatLongOperator();

		virtual std::unique_ptr<GenericRaster> getRaster(const QueryRectangle &rect, const QueryTools &toolss);
};
#endif



ProjectionOperator::ProjectionOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params) :
		RasterTimeSeriesOperator(sourcecounts, sources, params),
		src_crsId(CrsId::from_srs_string(params["src_projection"].asString())),
		dest_crsId(CrsId::from_srs_string(params["dest_projection"].asString())) {
	if (src_crsId == CrsId::unreferenced() || dest_crsId == CrsId::unreferenced())
		throw OperatorException("Unknown EPSG");
	assumeSources(1);
}

ProjectionOperator::~ProjectionOperator() {
}
REGISTER_OPERATOR(ProjectionOperator, "projection");

void ProjectionOperator::writeSemanticParameters(std::ostringstream &stream) {
	stream << "{\"src_projection\":" << src_crsId.to_string() << "\", \"dest_projection\":" << dest_crsId.to_string() << "\"}";
}

#ifndef MAPPING_OPERATOR_STUBS
template<typename T>
struct raster_projection {
	static std::unique_ptr<GenericRaster> execute(Raster2D<T> *raster_src, const GDAL::CRSTransformer *transformer, const SpatioTemporalReference &stref_dest, uint32_t width, uint32_t height) {
		raster_src->setRepresentation(GenericRaster::Representation::CPU);

		DataDescription out_dd = raster_src->dd;
		out_dd.addNoData();

		auto raster_dest_guard = GenericRaster::create(raster_src->dd, stref_dest, width, height);
		Raster2D<T> *raster_dest = (Raster2D<T> *) raster_dest_guard.get();

		T nodata = (T) out_dd.no_data;

		for (uint32_t y=0;y<raster_dest->height;y++) {
			for (uint32_t x=0;x<raster_dest->width;x++) {
				double px = raster_dest->PixelToWorldX(x);
				double py = raster_dest->PixelToWorldY(y);
				double pz = 0;

				if (!transformer->transform(px, py, pz)) {
					raster_dest->set(x, y, nodata);
					continue;
				}

				auto tx = raster_src->WorldToPixelX(px);
				auto ty = raster_src->WorldToPixelY(py);
				raster_dest->set(x, y, raster_src->getSafe(tx, ty, nodata));
			}
		}

		return raster_dest_guard;
	}
};

//GenericRaster *ProjectionOperator::execute(int timestamp, double x1, double y1, double x2, double y2, int xres, int yres) {
std::unique_ptr<GenericRaster> ProjectionOperator::getRaster(const QueryRectangle &rect, const QueryTools &tools) {
	if (dest_crsId != rect.crsId) {
		throw OperatorException("Projection: asked to transform to a different CRS than specified in QueryRectangle");
	}
	if (src_crsId == dest_crsId) {
		return getRasterFromSource(0, rect, tools);
	}

	GDAL::CRSTransformer transformer(dest_crsId, src_crsId);
    QueryRectangle src_rect = rect.project(src_crsId, transformer);

	auto raster_in = getRasterFromSource(0, src_rect, tools);

	if (src_crsId != raster_in->stref.crsId)
		throw OperatorException("ProjectionOperator: Source Raster not in expected projection");

	return callUnaryOperatorFunc<raster_projection>(raster_in.get(), &transformer, rect, rect.xres, rect.yres);
}


std::unique_ptr<rts::RasterTimeSeries> ProjectionOperator::getRasterTimeSeries(const QueryRectangle &rect, const QueryTools &tools, rts::ProcessingOrder processingOrder) {

	if(!supportsProcessingOrder(processingOrder))
		throw OperatorException(concat("RasterTimeSeries Projection does not support the processing order: ", processingOrder == rts::ProcessingOrder::Temporal ? "Temporal." : "Spatial."));

    if (dest_crsId != rect.crsId) {
        throw OperatorException("Projection: asked to transform to a different CRS than specified in QueryRectangle");
    }
    if (src_crsId == dest_crsId) {
        return getRasterTimeSeriesFromSource(0, rect, tools, processingOrder);
    }

    GDAL::CRSTransformer transformer(dest_crsId, src_crsId);
    QueryRectangle src_rect = rect.project(src_crsId, transformer);

    auto rts_in = getRasterTimeSeriesFromSource(0, src_rect, tools, processingOrder);

    auto state = std::make_unique<rts::ProjectionState>(std::move(rts_in), std::move(transformer), src_crsId, dest_crsId);

    return std::make_unique<rts::RasterTimeSeries>(this, std::move(state), rect, tools);
}

template<typename T>
struct raster_time_series_projection {

	static int64_t worldToPixelX(double worldX, double originX, double scaleX){
		return floor((worldX - originX) / scaleX);
	}

	static int64_t worldToPixelY(double worldY, double originY, double scaleY){
		return floor((worldY - originY) / scaleY);
	}

	static rts::Resolution toTwoDimensional(int tileIndex, const rts::Resolution &tileCount){
		return rts::Resolution(tileIndex % tileCount.resX, tileIndex / tileCount.resX);
	}

	static int toOneDimensional(const rts::Resolution &twoDimIndex, const rts::Resolution &tileCount){
		return twoDimIndex.resX + twoDimIndex.resY * tileCount.resX;
	}

	static std::unique_ptr<GenericRaster> execute(Raster2D<T> *raster_src, std::vector<rts::OptionalDescriptor> &descriptors, const SpatialReference &destTileInfo, const uint32_t tileIndex, const GDAL::CRSTransformer *transformer, const rts::Resolution &tileRes) {

		raster_src->setRepresentation(GenericRaster::Representation::CPU);

		rts::Scale scaleSrc(raster_src->pixel_scale_x, raster_src->pixel_scale_y);
		rts::Origin originSrc(raster_src->stref.x1, raster_src->stref.y1);

		DataDescription out_dd = raster_src->dd;
		out_dd.addNoData();

		SpatioTemporalReference stref_dest(destTileInfo, descriptors[tileIndex]->temporalInfo);

		rts::Resolution tileIndexTwoDim = toTwoDimensional(tileIndex,descriptors[tileIndex]->rasterTileCountDimensional);
		rts::Resolution tilePixelOffset(tileIndexTwoDim.resX * tileRes.resX, tileIndexTwoDim.resY * tileRes.resY);

		std::vector<std::unique_ptr<GenericRaster>> tiles;
		tiles.reserve(descriptors[tileIndex]->rasterTileCount);
		for (int i = 0; i < descriptors[tileIndex]->rasterTileCount; ++i) {
			tiles.emplace_back(nullptr);
		}

		auto raster_dest_guard = GenericRaster::create(raster_src->dd, stref_dest, tileRes.resX, tileRes.resY);
		Raster2D<T> *raster_dest = (Raster2D<T> *) raster_dest_guard.get();

		T nodata = (T) out_dd.no_data;

		for (uint32_t y=0;y<raster_dest->height;y++) {
			for (uint32_t x=0;x<raster_dest->width;x++) {
			    //calculate world position in destination raster
				double worldX = raster_dest->PixelToWorldX(x);
				double worldY = raster_dest->PixelToWorldY(y);
				double worldZ = 0;

				//transform to source projection
				if (!transformer->transform(worldX, worldY, worldZ)) {
					raster_dest->set(x, y, nodata);
					continue;
				}

				//calculate the cell in the source raster.
				auto tx = worldToPixelX(worldX, originSrc.x, scaleSrc.x);
				auto ty = worldToPixelY(worldY, originSrc.y, scaleSrc.y);

				if(tx >= 0 && tx < tileRes.resX && ty >= 0 && ty < tileRes.resY)
				{
					raster_dest->set(x, y, raster_src->getSafe(tx, ty, nodata));
				}
				else
                {
				    //value must be read from a different tile.

					//index delta. if tx or ty are smaller than 0 subtract 1 to get the real index delta.
					int dx = tx / tileRes.resX - (tx < 0 ? 1 : 0);
					int dy = ty / tileRes.resY - (ty < 0 ? 1 : 0);

					//get one dimensional index and load tile data for it if it is not already loaded.
					int idx = toOneDimensional(tileIndexTwoDim + rts::Resolution(dx, dy), descriptors[tileIndex]->rasterTileCountDimensional);
					if(!tiles[idx]){
						tiles[idx] = descriptors[idx]->getRasterCached();
					}

					//get pixel position in tile data.
					int tileX = tx % tileRes.resX;
                    int tileY = ty % tileRes.resY;
                    if(tx < 0)
                        tileX += tileRes.resX;
                    if(ty < 0)
                        tileY += tileRes.resY;

                    //read and write the value.
					auto *tile = (Raster2D<T> *)tiles[idx].get();
					raster_dest->set(x, y, tile->get(tileX, tileY));
				}
			}
		}

		return raster_dest_guard;
	}
};

bool ProjectionOperator::supportsProcessingOrder(rts::ProcessingOrder processingOrder) const {
    return processingOrder == rts::ProcessingOrder::Temporal;
}

rts::OptionalDescriptor ProjectionOperator::nextDescriptor(rts::OperatorState &state, const QueryRectangle &rect, const QueryTools &tools) {

    auto &s = state.castTo<rts::ProjectionState>();

    if(s.currentRaster.empty() || s.tileIndex >= s.currentRaster.size()){
        //if first descriptor or new raster: pre-load all input descriptors of the current raster into the list.
		s.tileIndex = 0;
		s.currentRaster.clear();
		s.currentRaster.emplace_back(s.input[0]->nextDescriptor());

		if(!s.currentRaster[0])
			return std::nullopt;

		for(int i = 1; i < s.currentRaster[0]->rasterTileCount; ++i){
			s.currentRaster.emplace_back(s.input[0]->nextDescriptor());
			if(s.currentRaster[i]->tileSpatialInfo.crsId != src_crsId){
                throw OperatorException("ProjectionOperator: Source Raster not in expected projection");
			}
		}
    }

    auto &descriptor_in = s.currentRaster[s.tileIndex];

    rts::DescriptorInfo info(descriptor_in, this);

    //adjust spatial info of complete raster and the tile:
	//spatial is taken from qrect, temporal is kept from input descriptor:
    info.rasterSpatialInfo = SpatialReference(rect);
    info.tileSpatialInfo = rts::RasterCalculations::tileIndexToSpatialRectangle(rect, s.tileIndex, descriptor_in->tileResolution);

    auto getter = [currentRaster = s.currentRaster, transformer = &s.transformer](const rts::Descriptor &self) -> std::unique_ptr<GenericRaster> {
    	auto raster_in = currentRaster[self.tileIndex]->getRasterCached();
		GenericRaster *raster_in_ptr = raster_in.get();
		return callUnaryOperatorFunc<raster_time_series_projection>(raster_in_ptr, currentRaster, self.tileSpatialInfo, self.tileIndex, transformer, self.tileResolution);
    };

	s.tileIndex += 1;

	return std::make_optional<rts::Descriptor>(std::move(getter), info);
}


std::unique_ptr<PointCollection> ProjectionOperator::getPointCollection(const QueryRectangle &rect, const QueryTools &tools) {
	if (dest_crsId != rect.crsId)
		throw OperatorException("Projection: asked to transform to a different CRS than specified in QueryRectangle");
	if (src_crsId == dest_crsId)
		return getPointCollectionFromSource(0, rect, tools);


	// Need to transform "backwards" to project the query rectangle..
	GDAL::CRSTransformer qrect_transformer(dest_crsId, src_crsId);
	QueryRectangle src_rect = rect.project(src_crsId, qrect_transformer);

	// ..but "forward" to project the points
	GDAL::CRSTransformer transformer(src_crsId, dest_crsId);


	auto points_in = getPointCollectionFromSource(0, src_rect, tools);

	if (src_crsId != points_in->stref.crsId) {
		std::ostringstream msg;
		msg << "ProjectionOperator: Source Points not in expected projection, expected " << src_crsId.to_string() << " got " << points_in->stref.crsId.to_string();
		throw OperatorException(msg.str());
	}

	std::vector<bool> keep(points_in->getFeatureCount(), true);
	bool has_filter = false;

	for(auto feature : *points_in){
		//project points in feature
		for(auto& coordinate : feature){
			if (!transformer.transform(coordinate.x, coordinate.y)) {
				//projection failed
				keep[feature] = false;
				has_filter = true;
				break;
			}
		}

		//check if feature is still in query rectangle
		if(keep[feature] && !points_in->featureIntersectsRectangle(feature, rect.x1, rect.y1, rect.x2, rect.y2)) {
			keep[feature] = false;
			has_filter = true;
		}
	}

	points_in->replaceSTRef(rect);

	if (!has_filter)
		return points_in;
	else
		return points_in->filter(keep);
}




std::unique_ptr<LineCollection> ProjectionOperator::getLineCollection(const QueryRectangle &rect, const QueryTools &tools) {
	if (dest_crsId != rect.crsId)
		throw OperatorException("Projection: asked to transform to a different CRS than specified in QueryRectangle");

	GDAL::CRSTransformer qrect_transformer(dest_crsId, src_crsId);
	QueryRectangle src_rect = rect.project(src_crsId, qrect_transformer);

	GDAL::CRSTransformer transformer(src_crsId, dest_crsId);

	auto lines_in = getLineCollectionFromSource(0, src_rect, tools);

	if (src_crsId != lines_in->stref.crsId) {
		std::ostringstream msg;
		msg << "ProjectionOperator: Source Lines not in expected projection, expected " << src_crsId.to_string() << " got " << lines_in->stref.crsId.to_string();
		throw OperatorException(msg.str());
	}

	std::vector<bool> keep(lines_in->getFeatureCount(), true);
	bool has_filter = false;

	for(auto feature : *lines_in){
		//project lines in feature
		for(auto line : feature){
			//project points in line
			for(auto& coordinate : line){
				if (!transformer.transform(coordinate.x, coordinate.y)) {
					//projection failed
					keep[feature] = false;
					has_filter = true;
					break;
				}
			}
			if(!keep[feature])
				break;
		}

		//check if feature is still in query rectangle
		if(keep[feature] && !lines_in->featureIntersectsRectangle(feature, rect.x1, rect.y1, rect.x2, rect.y2)){
			keep[feature] = false;
			has_filter = true;
		}
	}

	lines_in->replaceSTRef(rect);

	if (!has_filter)
		return lines_in;
	else
		return lines_in->filter(keep);
}

std::unique_ptr<PolygonCollection> ProjectionOperator::getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) {
	if (dest_crsId != rect.crsId)
		throw OperatorException("Projection: asked to transform to a different CRS than specified in QueryRectangle");

	GDAL::CRSTransformer qrect_transformer(dest_crsId, src_crsId);
	QueryRectangle src_rect = rect.project(src_crsId, qrect_transformer);

	GDAL::CRSTransformer transformer(src_crsId, dest_crsId);


	auto polygons_in = getPolygonCollectionFromSource(0, src_rect, tools);

	if (src_crsId != polygons_in->stref.crsId) {
		std::ostringstream msg;
		msg << "ProjectionOperator: Source Polygons not in expected projection, expected " << src_crsId.to_string() << " got " << polygons_in->stref.crsId.to_string();
		throw OperatorException(msg.str());
	}

	std::vector<bool> keep(polygons_in->getFeatureCount(), true);
	bool has_filter = false;

	for(auto feature : *polygons_in){
		//project polygons in feature
		for(auto polygon : feature){
			//project rings in polygon
			for(auto ring : polygon){
				//project points in ring
				for(auto& coordinate : ring){
					if (!transformer.transform(coordinate.x, coordinate.y)) {
						//projection failed
						keep[feature] = false;
						has_filter = true;
						break;
					}
				}
				if(!keep[feature])
					break;
			}
			if(!keep[feature])
				break;
		}

		//check if feature is still in query rectangle
		if(keep[feature] && !polygons_in->featureIntersectsRectangle(feature, rect.x1, rect.y1, rect.x2, rect.y2)){
			keep[feature] = false;
			has_filter = true;
		}
	}

	polygons_in->replaceSTRef(rect);

	if (!has_filter)
		return polygons_in;
	else
		return polygons_in->filter(keep);
}

#endif

#if 0
template<typename T>
struct meteosat_draw_latlong{
	static std::unique_ptr<GenericRaster> execute(Raster2D<T> *raster_src) {
		if (raster_src->lcrs.crsId != EPSG_METEOSAT2)
			throw OperatorException("Source raster not in meteosat projection");

		raster_src->setRepresentation(GenericRaster::Representation::CPU);

		T max = raster_src->dd.max*2;
		DataDescription vm_dest(raster_src->dd.datatype, raster_src->dd.min, max, raster_src->dd.has_no_data, raster_src->dd.no_data);
		auto raster_dest_guard = GenericRaster::create(raster_src->lcrs, vm_dest);
		Raster2D<T> *raster_dest = (Raster2D<T> *) raster_dest_guard.get();

		// erstmal alles kopieren
		int width = raster_dest->lcrs.size[0];
		int height = raster_dest->lcrs.size[1];
		for (int y=0;y<height;y++) {
			for (int x=0;x<width;x++) {
				raster_dest->set(x, y, raster_src->get(x, y));
			}
		}

		// LATLONG zeichnen
		for (float lon=-180;lon<=180;lon+=10) {
			for (float lat=-90;lat<=90;lat+=0.01) {
				int px, py;
				GDAL::CRSTransformer::msg_geocoord2pixcoord(lon, lat, &px, &py);
				if (px < 0 || py < 0)
					continue;
				raster_dest->set(px, py, max);
			}
		}


		for (float lon=-180;lon<=180;lon+=0.01) {
			for (float lat=-90;lat<=90;lat+=10) {
				int px, py;
				GDAL::CRSTransformer::msg_geocoord2pixcoord(lon, lat, &px, &py);
				if (px < 0 || py < 0)
					continue;
				raster_dest->set(px, py, max);
			}
		}

		return raster_dest_guard;
	}
};


MeteosatLatLongOperator::MeteosatLatLongOperator(int sourcecount, GenericOperator *sources[]) : GenericOperator(Type::RASTER, sourcecount, sources) {
	assumeSources(1);
}
MeteosatLatLongOperator::~MeteosatLatLongOperator() {
}

std::unique_ptr<GenericRaster> MeteosatLatLongOperator::getRaster(const QueryRectangle &rect, const QueryTools &tools) {
	auto raster_in = getRasterFromSource(0, rect, tools);

	return callUnaryOperatorFunc<meteosat_draw_latlong>(raster_in.get());
}

#endif


