
#include "operators/queryrectangle.h"
#include "util/exceptions.h"
#include "util/binarystream.h"

#include <algorithm>

/*
 * QueryResolution class
 */

QueryResolution::QueryResolution(BinaryReadBuffer &buffer) {
	buffer.read(&restype);
	buffer.read(&xres);
	buffer.read(&yres);
}

void QueryResolution::serialize(BinaryWriteBuffer &buffer, bool) const {
	buffer << restype;
	buffer << xres << yres;
}

/*
 * QueryRectangle class
 */
QueryRectangle::QueryRectangle(const GridSpatioTemporalResult &grid)
	: QueryRectangle(grid.stref, grid.stref, QueryResolution::pixels(grid.width, grid.height)) {
}


QueryRectangle::QueryRectangle(BinaryReadBuffer &buffer) : SpatialReference(buffer), TemporalReference(buffer), QueryResolution(buffer) {
}

void QueryRectangle::serialize(BinaryWriteBuffer &buffer, bool is_persistent_memory) const {
	SpatialReference::serialize(buffer, is_persistent_memory);
	TemporalReference::serialize(buffer, is_persistent_memory);
	QueryResolution::serialize(buffer, is_persistent_memory);
}

void QueryRectangle::enlargePixels(int pixels) {
	if (restype != QueryResolution::Type::PIXELS)
		throw ArgumentException("Cannot enlarge QueryRectangle without a proper pixel size");

	double pixel_size_in_world_coordinates_x = (double) (x2 - x1) / xres;
	double pixel_size_in_world_coordinates_y = (double) (y2 - y1) / yres;

	x1 -= pixels * pixel_size_in_world_coordinates_x;
	x2 += pixels * pixel_size_in_world_coordinates_x;
	y1 -= pixels * pixel_size_in_world_coordinates_y;
	y2 += pixels * pixel_size_in_world_coordinates_y;

	xres += 2*pixels;
	yres += 2*pixels;
}

void QueryRectangle::enlargeFraction(double fraction) {
	// If the desired resolution is specified in pixels, we would need to adjust the amount of requested pixels as well.
	// Until there's a use case for this, I'd rather not bother figuring out the best way to handle rounding.
	if (restype == QueryResolution::Type::PIXELS)
		throw ArgumentException("Cannot (yet) enlarge QueryRectangle by a fraction when a pixel size is present");

	double enlarge_x = (x2 - x1) * fraction;
	double enlarge_y = (y2 - y1) * fraction;

	x1 -= enlarge_x;
	x2 += enlarge_x;

	y1 -= enlarge_y;
	y2 += enlarge_y;
}

QueryRectangle QueryRectangle::project(const CrsId &targetCrs, const GDAL::CRSTransformer &transformer) const {
	// sample the borders of the query rectangle and project them to the source projection
	// calculate the bounding box and return it as the projected query rectangle
	auto samples = sample_borders(20);

	double minX = std::numeric_limits<double>::max();
	double minY = std::numeric_limits<double>::max();
	double maxX = -std::numeric_limits<double>::max();
	double maxY = -std::numeric_limits<double>::max();

	for (size_t i = 0; i < samples.size(); ++i) {
		Coordinate &s = samples[i];
		transformer.transform(s.x, s.y);
		minX = std::min(minX, s.x);
		maxX = std::max(maxX, s.x);
		minY = std::min(minY, s.y);
		maxY = std::max(maxY, s.y);
	}

	QueryRectangle result(
			SpatialReference(targetCrs, minX, minY, maxX, maxY),
			*this,
			restype == QueryResolution::Type::PIXELS ? QueryResolution::pixels(xres, yres) : QueryResolution::none()
	);
	return result;
}
