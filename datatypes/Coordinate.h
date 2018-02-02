#ifndef MAPPING_CORE_COORDINATE_H
#define MAPPING_CORE_COORDINATE_H

#include "util/binarystream.h"

#include <limits>
#include <cmath>

/**
 * This class models a 2 dimensional coordinate used in the feature collections
 */
class Coordinate {
public:
    Coordinate(BinaryReadBuffer &buffer);
    void serialize(BinaryWriteBuffer &buffer, bool is_persistent_memory) const;

    Coordinate(double x, double y) : x(x), y(y) {}

    Coordinate() = delete;
    ~Coordinate() = default;

    // Copy
    Coordinate(const Coordinate &p) = default;
    Coordinate &operator=(const Coordinate &p) = default;
    // Move
    Coordinate(Coordinate &&p) = default;
    Coordinate &operator=(Coordinate &&p) = default;

    bool almostEquals(const Coordinate& coordinate) const {
        return std::fabs(x - coordinate.x) < std::numeric_limits<double>::epsilon() && std::fabs(y - coordinate.y) < std::numeric_limits<double>::epsilon();
    }

    double x, y;

    friend class PointCollection;
    friend class LineCollection;
    friend class PolygonCollection;
    friend class GeosGeomUtil;
};


#endif //MAPPING_CORE_COORDINATE_H
