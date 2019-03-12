#include <gdal/ogrsf_frmts.h>
#include "operators/operator.h"
#include "datatypes/polygoncollection.h"
#include "datatypes/linecollection.h"
#include "datatypes/pointcollection.h"
#include "datatypes/simplefeaturecollection.h"
#include "util/ogr_source_util.h"
#include "util/exceptions.h"
#include "util/make_unique.h"
#include "util/timeparser.h"
#include "configuration.h"
#include <unordered_map>
#include <unordered_set>
#include <json/json.h>
#include <iostream>
#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>

OGRSourceUtil::OGRSourceUtil(Json::Value &params, const std::string &&local_id)
        : params(params), local_identifier(local_id)
{
    initialization(params);
}

OGRSourceUtil::OGRSourceUtil(Json::Value &&params, const std::string &&local_id)
        : params(params), local_identifier(local_id)
{
    initialization(this->params);
}

void OGRSourceUtil::initialization(Json::Value &params){
    //read the different parameters
    errorHandling = ErrorHandlingConverter.from_json(params, "on_error");

    Json::Value columns = params.get("columns", Json::Value(Json::ValueType::objectValue));
    auto textual = columns.get("textual", Json::Value(Json::ValueType::arrayValue));
    for (auto &name : textual)
        wantedAttributes[name.asString()] = AttributeType::TEXTUAL;

    auto numeric = columns.get("numeric", Json::Value(Json::ValueType::arrayValue));
    for (auto &name : numeric)
        wantedAttributes[name.asString()] = AttributeType::NUMERIC;

    if (!params.isMember("time"))
        throw ArgumentException("OGRSourceUtil: No time column specified.");

    timeSpecification = TimeSpecificationConverter.from_json(params, "time");

    timeDuration = 0.0;
    if (timeSpecification == TimeSpecification::START) {
        if (!params.isMember("duration"))
            throw ArgumentException("OGRSourceUtil: TimeSpecification::Start chosen, but no duration given.");

        auto duration = params.get("duration", Json::Value());
        if (duration.isString() && duration.asString() == "inf")
            timeDuration = -1.0;
        else if (duration.isNumeric())
            timeDuration = duration.asDouble();
        else
            throw ArgumentException("OGRSourceUtil: invalid duration given.");
    }

    if (timeSpecification != TimeSpecification::NONE) {
        time1Name = columns.get("time1", "time1").asString();

        const Json::Value &time1Format = params.get("time1_format", Json::Value::null);
        time1Parser = TimeParser::createFromJson(time1Format);
    }

    if (timeSpecification == TimeSpecification::START_DURATION || timeSpecification == TimeSpecification::START_END) {
        //TODO: check that time2 can be used as interval (e.g. format::seconds)
        time2Name = columns.get("time2", "time2").asString();

        const Json::Value &time2Format = params.get("time2_format", Json::Value::null);
        time2Parser = TimeParser::createFromJson(time2Format);
    }

    hasDefaultGeometry = params.isMember("default");
}

// Wrapper to read the different collection types. The passed function addFeature handles the specifics of the reading
// that differ for the different collection types. Handles the attribute and time reading for all collection types.
void OGRSourceUtil::readAnyCollection(const QueryRectangle &rect,
                                      SimpleFeatureCollection *collection,
                                      std::function<bool(OGRGeometry *)> addFeature) {
    //open GDALDataset and put the raw pointer into a unique_ptr member to handle it's lifetime.
    GDALDataset *dataset_raw = OGRSourceUtil::openGDALDataset(params);
    if (dataset_raw == nullptr) {
        throw OperatorException("OGR Source: Can not load dataset");
    }

    dataset = std::unique_ptr<GDALDataset, std::function<void(GDALDataset *)>>(
            dataset_raw,
            [](GDALDataset *dataset) {
                GDALClose(dataset);
            });
    dataset_raw = nullptr;

    if (dataset->GetLayerCount() < 1)
        throw OperatorException("OGR Source: No layers in OGR Dataset");

    //get the layer. We have no responsibility over it's memory.
    OGRLayer *layer = nullptr;
    if (params.isMember("layer_name"))
        layer = dataset->GetLayerByName(params["layer_name"].asCString());
    else
        layer = dataset->GetLayer(0);

    if (layer == nullptr)
        throw OperatorException("OGR Source: Layer could not be read from dataset.");

    // filters all Features not intersecting with the query rectangle.
    layer->SetSpatialFilterRect(rect.x1, rect.y1, rect.x2, rect.y2);

    // a call suggested by OGR Tutorial as "just in case" (to start reading from first feature)
    layer->ResetReading();

    // no need to process any further if result is empty
    if (layer->GetFeatureCount() < 1) {
        return;
    }

    //start reading the FeatureCollection
    OGRFeatureDefn *attributeDefn = layer->GetLayerDefn();
    createAttributeArrays(attributeDefn, collection->feature_attributes);
    initTimeReading(attributeDefn, layer, rect);

    //createFromWkt allocates a geometry and writes its pointer into a local pointer, therefore the third parameter is a OGRGeometry **.
    //afterwards it is moved into a unique_ptr
    OGRGeometry *default_geometry_raw = nullptr;
    if (hasDefaultGeometry) {
        OGRSpatialReference ref(nullptr);
        std::string wkt = params.get("default", "").asString();
        char *wktChar = (char *)wkt.c_str();
        OGRErr err = OGRGeometryFactory::createFromWkt(&wktChar, &ref, &default_geometry_raw);
        if (err != OGRERR_NONE)
            throw OperatorException("OGR Source: default wkt-string could not be parsed.");
    }

    std::unique_ptr<OGRGeometry, std::function<void(OGRGeometry *)>> default_geometry(
            default_geometry_raw,
            [](OGRGeometry *geom) {
                OGRGeometryFactory::destroyGeometry(geom);
            });
    default_geometry_raw = nullptr;


    int featureCount = 0;
    //add deleter
    OGRFeature *feature_raw = nullptr;
    auto feature_deleter = [](OGRFeature *feature) {
        OGRFeature::DestroyFeature(feature);
    };

    // iterate all features of the OGRLayer
    while ((feature_raw = layer->GetNextFeature()) != nullptr) {
        //move the feature_raw pointer into a unique_ptr with custom deleter.
        std::unique_ptr<OGRFeature, std::function<void(OGRFeature *)>> feature(feature_raw, feature_deleter);
        feature_raw = nullptr;

        //each feature has potentially multiple geometries
        if (feature->GetGeomFieldCount() > 1) {
            throw OperatorException(
                    "OGR Source: at least one OGRFeature has more than one Geometry. For now we don't support this.");
        }

        OGRGeometry *geom = feature->GetGeomFieldRef(0);
        bool success = false;

        if (geom != nullptr) {
            success = addFeature(geom);
        } else if (hasDefaultGeometry) {
            success = addFeature(default_geometry.get());
        }

        if (success) {
            //read time and if time reading was successful read attributes.
            bool time_and_attribute_success = readTimeIntoCollection(rect, feature.get(), collection->time);
            if (time_and_attribute_success)
                time_and_attribute_success = readAttributesIntoCollection(collection->feature_attributes, attributeDefn,
                                                                          feature.get(), featureCount);

            // false means that attribute or time could not be written and errorhandling is SKIP:
            // the already inserted feature has to be removed, rest of loop can be skipped.
            if (!time_and_attribute_success) {
                collection->removeLastFeature();
                continue;
            }
        }

        if (success)
            featureCount++;
        else {
            switch (errorHandling) {
                case ErrorHandling::ABORT:
                    if (geom == nullptr)
                        throw OperatorException(
                                "OGR Source: Invalid dataset, at least one OGRGeometry was NULL and no default values exist.");
                    else
                        throw OperatorException(
                                "OGR Source: Dataset contains not expected FeatureType (Points,Lines,Polygons)");
                    break;
                case ErrorHandling::SKIP:
                    // removing the last written feature is handled after the error occurred because here
                    // we can not know if writing has actually started or if a type mismatch happened before writing.
                    break;
                case ErrorHandling::KEEP:
                    //TODO: ???? Insert 0-Feature?
                    break;
            }
        }
    }

    // if it is wanted to insert default timestamps for TimeSpec::NONE uncomment this.
    // for now we decided not to add them.
    /*if(timeSpecification == TimeSpecification::NONE)
        collection->addDefaultTimestamps();*/
}

std::unique_ptr<PointCollection>
OGRSourceUtil::getPointCollection(const QueryRectangle &rect, const QueryTools &tools) {
    auto points = make_unique<PointCollection>(rect);

    auto addFeature = [&](OGRGeometry *geom) -> bool {
        int type = wkbFlatten(geom->getGeometryType());

        if (type == wkbPoint) {
            auto point = static_cast<OGRPoint *>(geom);
            points->addCoordinate(point->getX(), point->getY());
            points->finishFeature();
        } else if (type == wkbMultiPoint) {
            auto mp = static_cast<OGRMultiPoint *>(geom);
            int num = mp->getNumGeometries();
            for (int k = 0; k < num; k++) {
                auto point = static_cast<OGRPoint *>(mp->getGeometryRef(k));
                points->addCoordinate(point->getX(), point->getY());
                points->finishFeature();
            }
        } else
            return false;

        return true;
    };

    readAnyCollection(rect, points.get(), addFeature);
    points->validate();
    return points;
}

std::unique_ptr<LineCollection> OGRSourceUtil::getLineCollection(const QueryRectangle &rect, const QueryTools &tools) {
    auto lines = make_unique<LineCollection>(rect);

    auto addFeature = [&](OGRGeometry *geom) -> bool {
        int type = wkbFlatten(geom->getGeometryType());

        if (type == wkbLineString) {
            auto ls = static_cast<OGRLineString *>(geom);
            readLineStringToLineCollection(ls, lines);
            lines->finishFeature();
        } else if (type == wkbMultiLineString) {
            auto mls = static_cast<OGRMultiLineString *>(geom);
            for (int l = 0; l < mls->getNumGeometries(); l++) {
                auto ls = static_cast<OGRLineString *>(mls->getGeometryRef(l));
                readLineStringToLineCollection(ls, lines);
            }
            lines->finishFeature();
        } else
            return false;

        return true;
    };

    readAnyCollection(rect, lines.get(), addFeature);
    lines->validate();
    return lines;
}

std::unique_ptr<PolygonCollection>
OGRSourceUtil::getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) {
    auto polygons = make_unique<PolygonCollection>(rect);

    auto addFeature = [&](OGRGeometry *geom) -> bool {
        int type = wkbFlatten(geom->getGeometryType());
        if (type == wkbPolygon) {
            auto polygon = static_cast<OGRPolygon *>(geom);
            const OGRLinearRing *extRing = polygon->getExteriorRing();
            readRingToPolygonCollection(extRing, polygons);

            for (int k = 0; k < polygon->getNumInteriorRings(); k++) {
                const OGRLinearRing *intRing = polygon->getInteriorRing(k);
                readRingToPolygonCollection(intRing, polygons);
            }
            polygons->finishPolygon();
            polygons->finishFeature();

        } else if (type == wkbMultiPolygon || type == wkbMultiSurface) {
            //only supports multisurfaces containing polygons.
            auto multiPolygon = static_cast<OGRMultiSurface *>(geom);

            for (int l = 0; l < multiPolygon->getNumGeometries(); l++) {
                auto polygon = static_cast<OGRPolygon *>(multiPolygon->getGeometryRef(l));

                if(polygon->getGeometryType() != OGRwkbGeometryType::wkbPolygon){
                    if(l > 0){ //writing already started, so remove feature.
                        polygons->finishFeature();
                        polygons->removeLastFeature();
                    }
                    return false;
                }

                const OGRLinearRing *extRing = polygon->getExteriorRing();
                readRingToPolygonCollection(extRing, polygons);

                for (int k = 0; k < polygon->getNumInteriorRings(); k++) {
                    const OGRLinearRing *intRing = polygon->getInteriorRing(k);
                    readRingToPolygonCollection(intRing, polygons);
                }
                polygons->finishPolygon();
            }
            polygons->finishFeature();
        }  else
            return false;

        return true;
    };

    readAnyCollection(rect, polygons.get(), addFeature);
    polygons->validate();
    return polygons;
}

// reads all points from a OGRLineString into a LineCollection and finish the added feature
void
OGRSourceUtil::readLineStringToLineCollection(const OGRLineString *line, std::unique_ptr<LineCollection> &collection) {
    for (int l = 0; l < line->getNumPoints(); l++) {
        OGRPoint p;
        line->getPoint(l, &p);
        collection->addCoordinate(p.getX(), p.getY());
    }
    collection->finishLine();
}

// reads all points from a OGRLinearRing (Ring of a polygon) into a PolygonCollection and finish the added feature
void
OGRSourceUtil::readRingToPolygonCollection(const OGRLinearRing *ring, std::unique_ptr<PolygonCollection> &collection) {
    for (int l = 0; l < ring->getNumPoints(); l++) {
        OGRPoint p;
        ring->getPoint(l, &p);
        collection->addCoordinate(p.getX(), p.getY());
    }
    collection->finishRing();
}

// create the AttributeArrays for the FeatureCollection based on Attribute Fields in OGRLayer. 
// only if the user asked for the attribute in the query parameters.
// create a string vector with the attribute names for writing the attributes for each feature easier
void OGRSourceUtil::createAttributeArrays(OGRFeatureDefn *attributeDefn, AttributeArrays &attributeArrays) {
    int attributeCount = attributeDefn->GetFieldCount();
    std::unordered_set<std::string> existingAttributes;

    //iterate all attributes / field definitions
    for (int i = 0; i < attributeCount; i++) {
        OGRFieldDefn *fieldDefn = attributeDefn->GetFieldDefn(i);
        std::string name(fieldDefn->GetNameRef());
        if (name.empty()) {
            throw OperatorException("OGR Source: an attribute has no name.");
        }
        existingAttributes.insert(name);
        if (wantedAttributes.find(name) != wantedAttributes.end()) {
            AttributeType wantedType = wantedAttributes[name];
            attributeNames.push_back(name);
            //create numeric or textual attribute with the name in FeatureCollection
            if (wantedType == AttributeType::TEXTUAL)
                attributeArrays.addTextualAttribute(name, Unit::unknown()); //TODO: units
            else if (wantedType == AttributeType::NUMERIC)
                attributeArrays.addNumericAttribute(name, Unit::unknown()); //TODO: units
        } else
            attributeNames.emplace_back(
                    "");    //if attribute is not wanted add an empty string into the attributeNames vector.
    }

    // check if all requested attributes exist in FeatureDefn
    for (auto wanted : wantedAttributes) {
        if (existingAttributes.find(wanted.first) == existingAttributes.end())
            throw OperatorException(
                    "OGR Source: The requested attribute " + wanted.first + " does not exist in source file.");
    }
}


// write attribute values for the given attribute definition using the attributeNames
// returns false if an error occurred and ErrorHandling is set to skip so that 
// the last written feature has to be removed again in the calling function
bool OGRSourceUtil::readAttributesIntoCollection(AttributeArrays &attributeArrays, OGRFeatureDefn *attributeDefn,
                                                 OGRFeature *feature, int featureIndex) {
    for (int i = 0; i < attributeDefn->GetFieldCount(); i++) {
        //if attribute was not wanted the place in the std::vector has an empty string
        if (attributeNames[i].empty())
            continue;

        OGRFieldDefn *fieldDefn = attributeDefn->GetFieldDefn(i);
        AttributeType wantedType = wantedAttributes[attributeNames[i]];
        OGRFieldType type = fieldDefn->GetType();

        //attribute is read as numeric or textual depending on what the user asked for.
        if (wantedType == AttributeType::TEXTUAL) {
            //simply get the string representation of the attribute
            attributeArrays.textual(attributeNames[i]).set(featureIndex, feature->GetFieldAsString(i));
        } else if (wantedType == AttributeType::NUMERIC) {
            if (type == OFTInteger)
                attributeArrays.numeric(attributeNames[i]).set(featureIndex, feature->GetFieldAsInteger(i));
            else if (type == OFTInteger64)
                attributeArrays.numeric(attributeNames[i]).set(featureIndex, feature->GetFieldAsInteger64(i));
            else if (type == OFTReal)
                attributeArrays.numeric(attributeNames[i]).set(featureIndex, feature->GetFieldAsDouble(i));
            else {
                //the attribute type did not match any of the given number types. try to parse the string representation to a double
                try {
                    double parsed = std::stod(feature->GetFieldAsString(i));
                    attributeArrays.numeric(attributeNames[i]).set(featureIndex, parsed);
                }
                catch (...) {
                    switch (errorHandling) {
                        case ErrorHandling::ABORT:
                            throw OperatorException("OGR Source: Attribute \"" + attributeNames[i] +
                                                    "\" requested as numeric can not be parsed to double.");
                            break;
                        case ErrorHandling::SKIP:
                            return false;
                        case ErrorHandling::KEEP:
                            attributeArrays.numeric(attributeNames[i]).set(featureIndex, 0.0);
                            break;
                    }
                }
            }
        }
    }
    return true;
}

// initializes the column index for the time columns
void OGRSourceUtil::initTimeReading(OGRFeatureDefn *attributeDefn, OGRLayer *layer, const QueryRectangle &rect) {
    if (timeSpecification == TimeSpecification::NONE)
        return;

    time1Type = OGRFieldType::OFTInteger;
    time2Type = OGRFieldType::OFTInteger;
    time1Index = -1;
    time2Index = -1;

    for (int i = 0; i < attributeDefn->GetFieldCount(); i++) {
        OGRFieldDefn *fieldDefn = attributeDefn->GetFieldDefn(i);
        std::string name(fieldDefn->GetNameRef());
        if (name == time1Name) {
            time1Type = fieldDefn->GetType();
            time1Index = i;
        } else if (timeSpecification != TimeSpecification::START && name == time2Name) {
            time2Type = fieldDefn->GetType();
            time2Index = i;
        }
    }

    if (time1Index == -1)
        throw OperatorException("OGR Source: time1 attribute not found.");

    if (timeSpecification == TimeSpecification::START_DURATION || timeSpecification == TimeSpecification::START_END) {
        if (time2Index == -1)
            throw OperatorException("OGR Source: time2 attribute not found.");
    }

    //try filtering the attributes via OGRLayer, so we don't have to do it manually in readTimeIntoCollection
    timeAlreadyFiltered = trySettingTemporalFilter(layer, rect);
}

bool OGRSourceUtil::trySettingTemporalFilter(OGRLayer *layer, const QueryRectangle &rect) {
    // DateTime and Date are the supported field data type!
    // Both fields have to available and of type DateTime/Date, because only START_END as TimeSpecification is supported.
    // That's because I couldn't find a way to express in the sql like query (time_attribute + time_duration).
    if (timeSpecification != TimeSpecification::START_END)
        return false;

    // can try to set the filter without the fields being of type Date/DateTime. But that is unsafe and
    // can result in empty collections. So the filter could be set on string data fields and still be a good query.
    // but both attributes still have to be present and be of a Date/DateTime structure (as a string).
    bool hasSaveTypes = isDateOrDateTime(time1Type) && isDateOrDateTime(time2Type);

    // if this parameter is true, ignore if we have save types. e.g. WFS would still work, if the data on the wfs server
    // is Date/DateTime because the filtering happens there, but the file send to us will have String as data type,
    // because the WFS driver can not send it while keeping Date/DateTime datatypes.
    const bool forceOGRTimeFiltering = params.get("force_ogr_time_filter", false).asBool();

    if (!forceOGRTimeFiltering && !hasSaveTypes)
        return false;

    // now check if the fields are strings if force is used. Testing against Integer, Double wouldn't make sense
    if (!hasSaveTypes && forceOGRTimeFiltering &&
        (time1Type != OGRFieldType::OFTString || time2Type != OGRFieldType::OFTString))
        return false;

    //create TimeDate representation of rect.t1 and rect.t2
    std::string rect_time_String_1 = "\'";
    rect_time_String_1 += rect.toIsoString(rect.t1);
    rect_time_String_1 += "\'";

    std::string rect_time_string_2 = "\'";
    rect_time_string_2 += rect.toIsoString(rect.t2);
    rect_time_string_2 += "\'";

    std::stringstream filter_stream;
    // Create SQL like comparison to check if attributes validity intersects with the query rectangles times:
    // (feature.t1 >= rect.t1 || feature.t2 >= rect.t1) && (feature.t1 <= rect.t2 || feature.t2 <= rect.t2)
    filter_stream << "( " << time1Name << " >= " << rect_time_String_1;
    filter_stream << " OR " << time2Name << " >= " << rect_time_String_1 << " ) ";
    filter_stream << " AND ( " << time1Name << " <= " << rect_time_string_2;
    filter_stream << " OR " << time2Name << " <= " << rect_time_string_2 << " )";

    OGRErr err = layer->SetAttributeFilter(filter_stream.str().c_str());
    //err actually does not provide useful information, it does not test the query.
    return err == OGRSTCNone;
}

// reads the time values for the given feature from the time columns/attributes.
bool OGRSourceUtil::readTimeIntoCollection(const QueryRectangle &rect, OGRFeature *feature,
                                           std::vector<TimeInterval> &time) {
    if (timeSpecification == TimeSpecification::NONE)
        return true;

    // read time attributes
    double t1 = 0.0, t2 = 0.0;
    bool error = false;
    if (timeSpecification == TimeSpecification::START) {
        try {
            t1 = getTime1(feature);
            if (timeDuration >= 0.0)
                t2 = t1 + timeDuration;
            else
                t2 = rect.end_of_time();
        } catch (const TimeParseException &e) {
            t1 = rect.beginning_of_time();
            t2 = rect.end_of_time();
            error = true;
        }
    } else if (timeSpecification == TimeSpecification::START_END) {
        try {
            t1 = getTime1(feature);
        } catch (const TimeParseException &e) {
            t1 = rect.beginning_of_time();
            error = true;
        }
        try {
            t2 = getTime2(feature);
        } catch (const TimeParseException &e) {
            t2 = rect.end_of_time();
            error = true;
        }
    } else if (timeSpecification == TimeSpecification::START_DURATION) {
        try {
            t1 = getTime1(feature);
            t2 = getTime2(feature);
            t2 += t1;
        } catch (const TimeParseException &e) {
            t1 = rect.beginning_of_time();
            t2 = rect.end_of_time();
            error = true;
        }
    }

    if (error) {
        switch (errorHandling) {
            case ErrorHandling::ABORT:
                throw OperatorException("OGR Source: Could not parse time.");
            case ErrorHandling::SKIP:
                return false;
            case ErrorHandling::KEEP:
                break;
        }
    }

    if(!timeAlreadyFiltered){
        //check if features time stamps have overlap with query rectangle times
        if (t1 < rect.t1 && t2 < rect.t1 || t1 > rect.t2 && t2 > rect.t2)
            return false;
    }

    time.emplace_back(TimeInterval(t1, t2));
    return true;
}

double OGRSourceUtil::getTime1(OGRFeature *feature){
    if(isDateOrDateTime(time1Type)){
        return getFieldAsTime_t(feature, time1Index);
    } else {
        return time1Parser->parse(feature->GetFieldAsString(time1Index));
    }
}

double OGRSourceUtil::getTime2(OGRFeature *feature){
    if(isDateOrDateTime(time2Type)){
        return getFieldAsTime_t(feature, time2Index);
    } else {
        return time2Parser->parse(feature->GetFieldAsString(time2Index));
    }
}

bool OGRSourceUtil::isDateOrDateTime(OGRFieldType type){
    return type == OGRFieldType::OFTDateTime || type == OGRFieldType::OFTDate;
}

time_t OGRSourceUtil::getFieldAsTime_t(OGRFeature *feature, int featureIndex) {
    using namespace boost::posix_time;

    int d = 0, mon = 0, y = 0, h = 0, min = 0, s = 0, time_zone = 0;
    bool success = feature->GetFieldAsDateTime(featureIndex, &y, &mon, &d, &h, &min, &s, &time_zone);
    if (!success)
        throw OperatorException("OGRSource: Could not read time attribute.");

    ptime time(boost::gregorian::date(y, mon, d), hours(h) + minutes(min) + seconds(s));

    // Return values for time_zone flag are explained here:
    // http://www.gdal.org/classOGRFeature.html#a9cbf2fc45b5674265db25183aa93b36c
    if (time_zone == 0) //unknown, assume it as UTC
    {
        return to_time_t(time);
    } else if (time_zone == 1) //localtime
    {
        // Because we don't know when this happens and what it is supposed to mean in this context
        // (local computer time, or the local time where the data was recorded [what we don't know]) we throw an exception for now.
        throw OperatorException("OGRSource: Unexpected time zone value from the time attribute: \"localtime\"");
    } else {
        // time zone is explicitly given, 100 <-> +00:00, 104 <-> +01:00; 111 <-> +02:45; 96 <-> -01:00, etc
        // So every step represents 15 minutes.
        int zone_hours = (time_zone - 100) / 4;
        int zone_minutes = (time_zone - 100) % 4;

        ptime adjusted_time = time;
        if (zone_hours != 0) {
            adjusted_time -= hours(zone_hours);
        }
        if (zone_minutes != 0) {
            adjusted_time -= minutes(15 * zone_minutes);
        }
        return to_time_t(adjusted_time);
    }
}

void OGRSourceUtil::getProvenance(ProvenanceCollection &pc) {

    Json::Value &provenanceInfo = params["provenance"];
    pc.add(Provenance(provenanceInfo.get("citation", "").asString(),
                      provenanceInfo.get("license", "").asString(),
                      provenanceInfo.get("uri", "").asString(),
                      local_identifier));

}

GDALDataset *OGRSourceUtil::openGDALDataset(Json::Value &params) {
    GDALAllRegister();

    std::string filename = params["filename"].asString();
    Json::Value columns = params["columns"];

    bool isCsv = hasSuffix(filename, ".csv") || hasSuffix(filename, ".tsv");

    // if its a csv file we have to add open options to tell gdal what the geometry columns are.
    if (isCsv) {
        std::string column_x = columns.get("x", "x").asString();

        if (columns.isMember("y")) {
            std::string column_y = columns.get("y", "y").asString();
            std::string optX = "X_POSSIBLE_NAMES=" + column_x;
            std::string optY = "Y_POSSIBLE_NAMES=" + column_y;
            std::string type_detect = "AUTODETECT_TYPE=YES";
            const char *const strs[] = {optX.c_str(), optY.c_str(), type_detect.c_str(), NULL};
            return (GDALDataset *) GDALOpenEx(filename.c_str(), GDAL_OF_VECTOR, NULL, strs, NULL);
        } else {
            std::string opt = "GEOM_POSSIBLE_NAMES=" + column_x;
            std::string type_detect = "AUTODETECT_TYPE=YES";
            const char *const strs[] = {opt.c_str(), type_detect.c_str(), NULL};
            return (GDALDataset *) GDALOpenEx(filename.c_str(), GDAL_OF_VECTOR, NULL, strs, NULL);
        }
    } else
        return (GDALDataset *) GDALOpenEx(filename.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
}

bool OGRSourceUtil::hasSuffix(const std::string &str, const std::string &suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

Json::Value &OGRSourceUtil::getParameters() {
    return params;
}
