#include "services/ogcservice.h"
#include "datatypes/pointcollection.h"
#include "datatypes/linecollection.h"
#include "datatypes/polygoncollection.h"
#include "processing/queryprocessor.h"
#include "pointvisualization/CircleClusteringQuadTree.h"
#include "util/timeparser.h"
#include "util/enumconverter.h"
#include "util/log.h"

#include <string>
#include <cmath>
#include <map>
#include <sstream>
#include <json/json.h>
#include <utility>
#include <vector>

enum class WFSServiceType {
	GetCapabilities, GetFeature
};

static const std::vector< std::pair<Query::ResultType, std::string> > featureTypeMap = {
	std::make_pair(Query::ResultType::POINTS, "points"),
	std::make_pair(Query::ResultType::LINES, "lines"),
	std::make_pair(Query::ResultType::POLYGONS, "polygons"),
};

static EnumConverter<Query::ResultType> featureTypeConverter(featureTypeMap);

/**
 * Implementation of the OGC WFS standard http://www.opengeospatial.org/standards/wfs
 * It currently only supports our specific use cases
 */
class WFSService : public OGCService {
	public:
		using OGCService::OGCService;
		virtual ~WFSService() = default;

		virtual void run();

	private:
		void getCapabilities();
		void getFeature();

		// TODO: implement
		std::string describeFeatureType();
		std::string getPropertyValue();
		std::string listStoredQueries();
		std::string describeStoredQueries();

		// helper functions
		std::pair<Query::ResultType, std::string> parseTypeNames(const std::string &typeNames) const;
		std::unique_ptr<PointCollection> clusterPoints(const PointCollection &points, const Parameters &params) const;

		const std::map<std::string, WFSServiceType> stringToRequest {
			{"GetCapabilities", WFSServiceType::GetCapabilities},
			{"GetFeature", WFSServiceType::GetFeature}
		};


};
REGISTER_HTTP_SERVICE(WFSService, "WFS");


void WFSService::run() {
	if (!params.hasParam("version") || params.get("version") != "2.0.0") {
		response.send500("wrong version");
	}

	switch (stringToRequest.at(params.get("request"))) {
	case WFSServiceType::GetCapabilities:
		getCapabilities();

		break;
	case WFSServiceType::GetFeature:
		getFeature();

		break;
	default:
		response.send500("wrong request");
		break;
	}
}

void WFSService::getCapabilities() {
	response.sendContentType("text/html");
	response.finishHeaders();
}

void WFSService::getFeature() {
	auto session = UserDB::loadSession(params.get("sessiontoken"));
	auto user = session->getUser();
	Log::debug("User: " + user.getUserIDString());

	if(!params.hasParam("typenames"))
		throw ArgumentException("WFSService: typeNames parameter not specified");

	auto typeNames = parseTypeNames(params.get("typenames"));
	auto resultType = typeNames.first;
	auto &operatorgraph = typeNames.second;

	TemporalReference tref = parseTime(params);

	// srsName=CRS
	// this parameter is optional in WFS, but we use it here to create the Spatial Reference.
	if(!params.hasParam("srsname"))
		throw ArgumentException("WFSService: Parameter srsname is missing");
	CrsId queryEpsg = parseCrsId(params, "srsname");


	SpatialReference sref(queryEpsg);
	if(params.hasParam("bbox")) {
		sref = parseBBOX(params.get("bbox"), queryEpsg);
	}

	Query query(operatorgraph, resultType, QueryRectangle(sref, tref, QueryResolution::none()));
	auto result = processQuery(query, user);
	auto features = result->getAnyFeatureCollection();

	// TODO: check permission

	//clustered is ignored for non-point collections
	//TODO: implement this as VSP or other operation?
	if (params.hasParam("clustered") && params.getBool("clustered", false) && resultType == Query::ResultType::POINTS) {
		PointCollection& points = dynamic_cast<PointCollection&>(*features);

		features = clusterPoints(points, params);
	}


	// TODO: startIndex + count
	// TODO: sortBy=attribute  +D or +A
	// push-down?

	// propertyName <- restrict attribute(s)

	// TODO: FILTER (+ FILTER_LANGUAGE)
	// <Filter>
	// 		<Cluster>
	//			<PropertyName>ResolutionHeight</PropertyName><Literal>1234</Literal>
	// 			<PropertyName>ResolutionWidth</PropertyName><Literal>1234</Literal>
	//		</Cluster>
	// </Filter>



	// resolve : ResolveValue = #none  (local+ remote+ all+ none)
	// resolveDepth : UnlimitedInteger = #isInfinite
	// resolveTimeout : TM_Duration = 300s
	//
	// {resolveDepth>0 implies resolve<>#none
	// and resolveTimeout->notEmpty() implies resolve<>#none}

	// resultType =  hits / results

	// outputFormat
	// default is "application/gml+xml; version=3.2"

	//TODO: respect default output format of WFS and support more datatypes
	auto format = params.get("outputformat", "application/json");

	bool exportMode = false;
	if(format.find(EXPORT_MIME_PREFIX) == 0) {
		exportMode = true;
		format = format.substr(strlen(EXPORT_MIME_PREFIX));
	}

	std::string output;
	if (format == "application/json")
		output = features->toGeoJSON(true);
	else if (format == "csv")
		output = features->toCSV();
	else
		throw ArgumentException("WFSService: unknown output format");

	if(exportMode) {
		exportZip(operatorgraph, output.c_str(), output.length(), format, result->getProvenance());
	} else {
		response.sendContentType(format + "; charset=utf-8");
		response.finishHeaders();
		response << output;
	}
	// VSPs
	// O
	// A server may implement additional KVP parameters that are not part of this International Standard. These are known as vendor-specific parameters. VSPs allow vendors to specify additional parameters that will enhance the results of requests. A server shall produce valid results even if the VSPs are missing, malformed or if VSPs are supplied that are not known to the server. Unknown VSPs shall be ignored.
	// A server may choose not to advertise some or all of its VSPs. If VSPs are included in the Capabilities XML (see 8.3), the ows:ExtendedCapabilities element (see OGC 06-121r3:2009, 7.4.6) shall be extended accordingly. Additional schema documents may be imported containing the extension(s) of the ows:ExtendedCapabilities element. Any advertised VSP shall include or reference additional metadata describing its meaning (see 8.4). WFS implementers should choose VSP names with care to avoid clashes with WFS parameters defined in this International Standard.
}



std::unique_ptr<PointCollection> WFSService::clusterPoints(const PointCollection &points, const Parameters &params) const {

	if(!params.hasParam("resolution")) {
		throw ArgumentException("WFSService: Cluster operation needs a resolution specified");
	}

	double resolution;

	try {
		resolution = std::stod(params.get("resolution"));
	} catch (const std::invalid_argument &e) {
		throw ArgumentException("WFSService: resolution parameter must be a double value");
	}

	if (resolution <= 0) {
		throw ArgumentException("WFSService: resolution is invalid (must be positive)");
	}

    // set default values for parameters
	double circle_min_radius_px = 5;
	double inter_circle_min_distance_px = 1;

    // try to look up new `circle_min_radius_px`
    const std::string circle_min_radius_px_key {"clustered_min_radius"};
	if (params.hasParam(circle_min_radius_px_key)) {
		try {
			circle_min_radius_px = std::stod(params.get(circle_min_radius_px_key));

            if (circle_min_radius_px <= 0) {
                throw ArgumentException(concat("WFSService: `", circle_min_radius_px_key, "` parameter must be > 0"));
            }
		} catch (const std::invalid_argument &e) {
			throw ArgumentException(concat("WFSService: `", circle_min_radius_px_key, "` parameter must be a double value"));
		}
	}

    // try to look up new `inter_circle_min_distance_px`
    const std::string inter_circle_min_distance_px_key {"clustered_min_distance"};
    if (params.hasParam(inter_circle_min_distance_px_key)) {
        try {
            inter_circle_min_distance_px = std::stod(params.get(inter_circle_min_distance_px_key));

            if (inter_circle_min_distance_px < 0) {
                throw ArgumentException(concat("WFSService: `", inter_circle_min_distance_px, "` parameter must be >= 0"));
            }
        } catch (const std::invalid_argument &e) {
            throw ArgumentException(concat("WFSService: `", inter_circle_min_distance_px_key, "` parameter must be a double value"));
        }
    }

	auto clusteredPoints = make_unique<PointCollection>(points.stref);

	const SpatialReference &sref = points.stref;
	auto x1 = sref.x1;
	auto x2 = sref.x2;
	auto y1 = sref.y1;
	auto y2 = sref.y2;

    // initialize empty tree
    std::size_t node_capacity = 1;
    pv::Circle::CommonAttributes common_attributes{circle_min_radius_px, inter_circle_min_distance_px};
	pv::CircleClusteringQuadTree clustering_tree{
        pv::BoundingBox(
            pv::Coordinate((x2 + x1) / (2 * resolution), (y2 + y1) / (2 * resolution)),
            pv::Dimension((x2 - x1) / (2 * resolution), (y2 - y1) / (2 * resolution)),
            common_attributes.getEpsilonDistance()
        ),
        node_capacity
    };

    // add coordinate by coordinate
    std::vector<std::string> text_keys = points.feature_attributes.getTextualKeys();
    std::vector<std::string> numeric_keys = points.feature_attributes.getNumericKeys();
    for (const auto& point: points) {
        Coordinate mapping_coordinate = points.coordinates.at(static_cast<std::size_t>(point));
        pv::Coordinate coordinate{mapping_coordinate.x / resolution, mapping_coordinate.y / resolution};

        // add text attributes
        std::map<std::string, pv::Circle::TextAttribute> text_attributes;
        for (const auto& key : text_keys) {
            text_attributes.insert(
                std::make_pair(
                    key,
                    pv::Circle::TextAttribute{
                        points.feature_attributes.textual(key).get(point),
                        coordinate,
                        common_attributes
                    }
                )
            );
        }

        // add numeric attributes
        std::map<std::string, pv::Circle::NumericAttribute> numeric_attributes;
        for (const auto& key : numeric_keys) {
            numeric_attributes.insert(
                std::make_pair(
                    key,
                    std::move(pv::Circle::NumericAttribute{points.feature_attributes.numeric(key).get(point)})
                )
            );
        }

        clustering_tree.insert(
            std::make_shared<pv::Circle>(
                coordinate,
                common_attributes,
                text_attributes,
                numeric_attributes
            )
        );
    }

	// PROPERTYNAME
	// O
	// A list of non-mandatory properties to include in the response.
	// TYPENAMES=(ns1:F1,ns2:F2)(ns1:F1,ns1:F1)&ALIASES=(A,B)(C,D)&FILTER=(<Filter> … for A,B … </Filter>)(<Filter>…for C,D…</Filter>)
	// TYPENAMES=ns1:F1,ns2:F2&ALIASES=A,B&FILTER=<Filter>…for A,B…</Filter>
	// TYPENAMES=ns1:F1,ns1:F1&ALIASES=C,D&FILTER=<Filter>…for C,D…</Filter>

    // output circles
	auto circles = clustering_tree.getCircles();

    // initialize map specific data
	auto &attr_radius = clusteredPoints->feature_attributes.addNumericAttribute("___radius", Unit::unknown());
	auto &attr_number = clusteredPoints->feature_attributes.addNumericAttribute("___numberOfPoints", Unit::unknown());
	attr_radius.reserve(circles.size());
	attr_number.reserve(circles.size());

    // initialize textual attributes
    for (const auto& key: text_keys) {
        clusteredPoints->feature_attributes.addTextualAttribute(key, points.feature_attributes.textual(key).unit);
    }

    // initialize numeric attributes as text attributes for output
    for (const auto& key: numeric_keys) {
        clusteredPoints->feature_attributes.addTextualAttribute(key, points.feature_attributes.numeric(key).unit);
    }

	for (const auto& circle : circles) {
        // add feature to point collection
		size_t idx = clusteredPoints->addSinglePointFeature(
            Coordinate(circle->getX() * resolution, circle->getY() * resolution)
        );

        // set circle map specific data
		attr_radius.set(idx, circle->getRadius());
		attr_number.set(idx, circle->getNumberOfPoints());

        // add textual attributes
        for (const auto& text_attribute_entry : circle->getTextAttributes()) {
            std::vector<std::string> texts = text_attribute_entry.second.getTexts();
            std::string texts_concat;
            for (const auto &text : text_attribute_entry.second.getTexts()) {
                texts_concat += text + ", ";
            }
            clusteredPoints->feature_attributes.textual(text_attribute_entry.first).set(
                idx,
                texts.size() >= 5 ? texts_concat + "…" : texts_concat.substr(0, texts_concat.size() - 2)
            );
        }

        // add numeric attributes as text attributes for output
        for (const auto& numeric_attribute_entry : circle->getNumericAttributes()) {
            double average = numeric_attribute_entry.second.getAverage();
            double variance = numeric_attribute_entry.second.getVariance();

            std::stringstream output_stream;

            if (!std::isnan(average)) {
                output_stream << average;

                if (variance > 0) {
                    output_stream << " ± " << sqrt(variance);
                }
            }

            clusteredPoints->feature_attributes.textual(numeric_attribute_entry.first).set(idx, output_stream.str());
        }
	}

	return clusteredPoints;
}

std::pair<Query::ResultType, std::string> WFSService::parseTypeNames(const std::string &typeNames) const {
	// the typeNames parameter specifies the requested layer : typeNames=namespace:featuretype
	// for now the namespace specifies the type of feature (points, lines, polygons) while the featuretype specifies the query

	//split typeNames string

	size_t pos = typeNames.find(":");

	if(pos == std::string::npos)
		throw ArgumentException(concat("WFSService: typeNames delimiter not found", typeNames));

	std::string featureTypeString = typeNames.substr(0, pos);
	std::string queryString = typeNames.substr(pos + 1);

	if(featureTypeString == "")
		throw ArgumentException("WFSService: featureType in typeNames not specified");
	if(queryString == "")
		throw ArgumentException("WFSService: query in typenNames not specified");

	Query::ResultType resultType = featureTypeConverter.from_string(featureTypeString);

	return std::make_pair(resultType, queryString);
}
