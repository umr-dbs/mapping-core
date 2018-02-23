#include "datatypes/spatiotemporal.h"
#include "util/exceptions.h"
#include "operators/operator.h"
#include "util/binarystream.h"

#include <limits>
#include <iomanip>
#include <ogr_spatialref.h>


/**
 * CrsId
 */
CrsId::CrsId(std::string authority, uint32_t code) : authority(std::move(authority)), code(code) {}

CrsId CrsId::from_epsg_code(uint32_t epsg_code) {
    return CrsId("EPSG", epsg_code);
}

CrsId CrsId::unreferenced() {
    return CrsId("UNREFERENCED", 0);
}

std::string CrsId::to_string() const {
    return concat(authority, ":", code);
}

CrsId::CrsId(BinaryReadBuffer &buffer) {
    buffer.read(&authority);
    buffer.read(&code);
}

void CrsId::serialize(BinaryWriteBuffer &buffer, bool) const {
    buffer << authority << code;
}

bool CrsId::operator==(const CrsId &other) const {
    return (authority == other.authority) && (code == other.code);
}

bool CrsId::operator!=(const CrsId &other) const {
    return !(*this == other);
}

CrsId CrsId::from_srs_string(const std::string &srsString) {
    size_t pos = srsString.find(":");

    if (pos == std::string::npos) {
        throw ArgumentException("Invalid CRS specified");
    }

    std::string authority = srsString.substr(0, pos);

    auto code = static_cast<uint32_t>(std::stoi(srsString.substr(pos + 1)));

    return CrsId(authority, code);
}

CrsId CrsId::from_wkt(const std::string &wkt) {
    OGRSpatialReference sref = OGRSpatialReference(wkt.c_str());
    return CrsId(std::string(sref.GetAuthorityName("GEOGCS")),
                 static_cast<uint32_t>(std::stoi(sref.GetAuthorityCode("GEOGCS"))));
}

CrsId CrsId::web_mercator() {
    return CrsId::from_epsg_code(3857);
}

CrsId CrsId::wgs84() {
    return CrsId::from_epsg_code(4326);
}


/**
 * SpatialReference
 */
SpatialReference::SpatialReference(CrsId crsId) : crsId(crsId) {
    auto e = extent(crsId);
    x1 = e.x1;
    x2 = e.x2;
    y1 = e.y1;
    y2 = e.y2;

    validate();
}

SpatialReference::SpatialReference(CrsId crsId, double x1, double y1, double x2, double y2)
        : crsId(std::move(crsId)), x1(x1), y1(y1), x2(x2), y2(y2) {
    validate();
}

SpatialReference::SpatialReference(CrsId crsId, double x1, double y1, double x2, double y2, bool &flipx, bool &flipy)
        : crsId(std::move(crsId)), x1(x1), y1(y1), x2(x2), y2(y2) {
    flipx = flipy = false;
    if (x1 > x2) {
        flipx = true;
        std::swap(this->x1, this->x2);
    }
    if (y1 > y2) {
        flipy = true;
        std::swap(this->y1, this->y2);
    }
    validate();
}

SpatialReference::SpatialReference(BinaryReadBuffer &buffer) : crsId(CrsId(buffer)) {
    buffer.read(&x1);
    buffer.read(&y1);
    buffer.read(&x2);
    buffer.read(&y2);

    validate();
}

void SpatialReference::serialize(BinaryWriteBuffer &buffer, bool is_persistent_buffer) const {
    crsId.serialize(buffer, is_persistent_buffer);
    buffer << x1 << y1 << x2 << y2;
}

/*
 * Returns whether the other SpatialReference is contained (smaller or equal) within this.
 * Throws an exception if the crs don't match
 */
bool SpatialReference::contains(const SpatialReference &other) const {
    if (crsId != other.crsId)
        throw ArgumentException("SpatialReference::contains(): crsId don't match");
    if (x1 <= other.x1 && x2 >= other.x2 && y1 <= other.y1 && y2 >= other.y2)
        return true;

    //TODO: Talk about this
    auto ex = SpatialReference::extent(crsId);
    double xeps = (ex.x2 - ex.x1) * std::numeric_limits<double>::epsilon();
    double yeps = (ex.y2 - ex.y1) * std::numeric_limits<double>::epsilon();

    return ((x1 - other.x1) < xeps) &&
           ((other.x2 - x2) < xeps) &&
           ((y1 - other.y1) < yeps) &&
           ((other.y2 - y2) < yeps);
}


void SpatialReference::validate() const {
    if (x1 > x2 || y1 > y2) {
        std::stringstream msg;
        msg << "SpatialReference invalid, requires x1:" << x1 << " <= x2:" << x2 << ", y1:" << y1 << " <= y2:" << y2;
        throw ArgumentException(msg.str());
    }
}

SpatialReference SpatialReference::extent(CrsId crsId) {
    // WebMercator, http://www.easywms.com/easywms/?q=en/node/3592
    if (crsId == CrsId::from_epsg_code(3857))
        return SpatialReference(CrsId::from_epsg_code(3857), -20037508.34, -20037508.34, 20037508.34, 20037508.34);
    if (crsId == CrsId::from_epsg_code(4326))
        return SpatialReference(CrsId::from_epsg_code(4326), -180, -90, 180, 90);
    if (crsId == CrsId::from_srs_string("SR-ORG:81"))
        return SpatialReference(CrsId::from_srs_string("SR-ORG:81"), -5568748.276, -5568748.276, 5568748.276,
                                5568748.276);

    return SpatialReference(crsId, -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
                            -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
}


/**
 * TimeInterval
 */

TimeInterval::TimeInterval(double t1, double t2) : t1(t1), t2(t2) {
    validate();
}

TimeInterval::TimeInterval(BinaryReadBuffer &buffer) {
    buffer.read(&t1);
    buffer.read(&t2);

    validate();
}

void TimeInterval::serialize(BinaryWriteBuffer &buffer, bool) const {
    buffer << t1 << t2;
}

void TimeInterval::validate() const {
    if (t1 >= t2)
        throw ArgumentException(concat("TimeInterval invalid, requires t1:", t1, " < t2:", t2));
}

void TimeInterval::validate(double beginning_of_time, double end_of_time) const {
    validate();
    if (t1 < beginning_of_time)
        throw ArgumentException(concat("TimeInterval invalid, requires t1:", t1, " >= bot:", beginning_of_time));
    if (t2 > end_of_time)
        throw ArgumentException(concat("TimeInterval invalid, requires t2:", t2, " <= eot:", end_of_time));
}


bool TimeInterval::contains(const TimeInterval &other) const {
    return t1 <= other.t1 && t2 >= other.t2;
}

bool TimeInterval::intersects(const TimeInterval &other) const {
    return intersects(other.t1, other.t2);
}

bool TimeInterval::intersects(double t_start, double t_end) const {
    return t_start < this->t2 && t_end > this->t1;
}

void TimeInterval::intersect(const TimeInterval &other) {
    t1 = std::max(t1, other.t1);
    t2 = std::min(t2, other.t2);
    if (t1 >= t2)
        throw ArgumentException("intersect(): both TimeIntervals do not intersect");
}

TimeInterval TimeInterval::intersection(const TimeInterval &other) {
    double intersectiont1 = std::max(t1, other.t1);
    double intersectiont2 = std::min(t2, other.t2);
    if (intersectiont1 >= intersectiont2)
        throw ArgumentException("intersect(): both TimeIntervals do not intersect");
    return TimeInterval(intersectiont1, intersectiont2);
}

void TimeInterval::union_with(TimeInterval &other) {
    if (!intersects(other))
        throw ArgumentException("union_with() both TimeIntervals do not intersect");

    t1 = std::min(t1, other.t1);
    t2 = std::max(t2, other.t2);
}

size_t TimeInterval::get_byte_size() const {
    return sizeof(TimeInterval);
}

/**
 * TemporalReference
 */

size_t TemporalReference::get_byte_size() const {
    return sizeof(TemporalReference);
}

TemporalReference::TemporalReference(timetype_t timetype) : TimeInterval(), timetype(timetype) {
    t1 = beginning_of_time();
    t2 = end_of_time();

    validate();
}

TemporalReference::TemporalReference(timetype_t timetype, double t1) : TimeInterval(), timetype(timetype) {
    this->t1 = t1;
    this->t2 = t1 + epsilon();
    if (this->t1 >= this->t2)
        throw MustNotHappenException(
                concat("TemporalReference::epsilon() too small for this magnitude, ", this->t1, " == ", this->t2));

    validate();
}

TemporalReference::TemporalReference(timetype_t timetype, double t1, double t2)
        : TimeInterval(t1, t2), timetype(timetype) {
    validate();
}


TemporalReference::TemporalReference(BinaryReadBuffer &buffer) : TimeInterval(buffer) {
    timetype = (timetype_t) buffer.read<uint32_t>();;

    validate();
}

void TemporalReference::serialize(BinaryWriteBuffer &buffer, bool is_persistent_memory) const {
    TimeInterval::serialize(buffer, is_persistent_memory);
    buffer << (uint32_t) timetype;
}


void TemporalReference::validate() const {
    TimeInterval::validate(beginning_of_time(), end_of_time());
}


double TemporalReference::beginning_of_time() const {
    if (timetype == TIMETYPE_UNIX) {
        // A test in unittests/temporal/timeparser.cpp verifies that the constant matches the given date
        //ISO 8601: 0001-01-01T00:00:00
        return -62135596800;
    }
    // The default for other timetypes is -infinity
    return -std::numeric_limits<double>::infinity();
}

double TemporalReference::end_of_time() const {
    if (timetype == TIMETYPE_UNIX) {
        // A test in unittests/temporal/timeparser.cpp verifies that the constant matches the given date
        //ISO 8601: 9999-12-31T23:59:59
        return 253402300799;
    }
    // The default for other timetypes is infinity
    return std::numeric_limits<double>::infinity();
}

double TemporalReference::epsilon() const {
    if (timetype == TIMETYPE_UNIX) {
        return 1.0 / 1000.0; // 1 millisecond should be small enough.
    }
    throw ArgumentException(concat("TemporalReference::epsilon() on unknown timetype ", (int) timetype, "\n"));
}


bool TemporalReference::contains(const TemporalReference &other) const {
    if (timetype != other.timetype)
        throw ArgumentException("TemporalReference::contains(): timetypes don't match");

    return TimeInterval::contains(other);
}

bool TemporalReference::intersects(const TemporalReference &other) const {
    if (timetype != other.timetype)
        throw ArgumentException("TemporalReference::contains(): timetypes don't match");

    return TimeInterval::intersects(other);
}

bool TemporalReference::intersects(double t_start, double t_end) const {
    return TimeInterval::intersects(t_start, t_end);
}


void TemporalReference::intersect(const TemporalReference &other) {
    if (timetype != other.timetype)
        throw ArgumentException("Cannot intersect() TemporalReferences with different timetype");

    TimeInterval::intersect(other);
}

std::string TemporalReference::toIsoString(double time) const {
    std::ostringstream result;

    if (timetype == TIMETYPE_UNIX) {
        if (time < beginning_of_time() || time > end_of_time())
            throw ArgumentException("toIsoString: given timestamp is outside the valid range");

        long t = time;
        std::tm *tm = std::gmtime(&t);

        if (tm == NULL)
            throw ArgumentException("Could not convert time to IsoString");

        int year = 1900 + tm->tm_year;
        int month = tm->tm_mon + 1;
        int day = tm->tm_mday;
        int hour = tm->tm_hour;
        int minute = tm->tm_min;
        int second = tm->tm_sec;

        double frac = time - t;

        result.fill('0');

        result << std::setw(4) << year << "-" << std::setw(2) << month << "-" << std::setw(2) << day << "T" <<
               std::setw(2) << hour << ":" << std::setw(2) << minute << ":" << std::setw(2) << second;

        if (frac > 0.0) {
            //remove leading zero
            std::stringstream ss;
            ss << frac;
            std::string frac_string = ss.str();

            frac_string.erase(0, frac_string.find_first_not_of('0'));

            result << frac_string;
        }

        //TODO: incorporate fractions of seconds
    } else {
        throw ConverterException("can only convert UNIX timestamps");
    }

    return result.str();
}


size_t SpatialReference::get_byte_size() const {
    return sizeof(SpatialReference);
}

std::vector<Coordinate> SpatialReference::sample_borders(size_t numberOfSamples) const {
    size_t borderSamples = numberOfSamples / 4;
    std::vector<Coordinate> samples;

    double dx = (x2 - x1) / borderSamples;
    double dy = (y2 - y1) / borderSamples;

    for (size_t i = 0; i < borderSamples; ++i) {
        samples.emplace_back(x1 + i * dx, y1);
        samples.emplace_back(x2, y1 + i * dy);
        samples.emplace_back(x2 - i * dx, y2);
        samples.emplace_back(x1, y2 - i * dy);
    }

    return samples;
}

/**
 * SpatioTemporalReference
 */
SpatioTemporalReference::SpatioTemporalReference(BinaryReadBuffer &buffer) : SpatialReference(buffer),
                                                                             TemporalReference(buffer) {
}

void SpatioTemporalReference::serialize(BinaryWriteBuffer &buffer, bool is_persistent_memory) const {
    SpatialReference::serialize(buffer, is_persistent_memory);
    TemporalReference::serialize(buffer, is_persistent_memory);
}

SpatioTemporalReference::SpatioTemporalReference(const QueryRectangle &rect) : SpatialReference(rect),
                                                                               TemporalReference(rect) {
}

void SpatioTemporalReference::validate() const {
    SpatialReference::validate();
    TemporalReference::validate();
}

size_t SpatioTemporalReference::get_byte_size() const {
    return sizeof(SpatioTemporalReference);
}


/**
 * SpatioTemporalResult
 */
void SpatioTemporalResult::replaceSTRef(const SpatioTemporalReference &newstref) {
    const_cast<SpatioTemporalReference &>(this->stref) = newstref;
}

size_t SpatioTemporalResult::get_byte_size() const {
    return stref.get_byte_size() + global_attributes.get_byte_size();
}

size_t GridSpatioTemporalResult::get_byte_size() const {
    return SpatioTemporalResult::get_byte_size() + 2 * sizeof(double) + 2 * sizeof(uint32_t);
}
