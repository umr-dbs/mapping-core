#include <iostream>
#include "featurecollectiondb/featurecollectiondbbackend.h"
#include "datatypes/simplefeaturecollection.h"
#include "processing/query.h"
#include "datatypes/simplefeaturecollections/wkbutil.h"
#include "util/exceptions.h"
#include "util/enumconverter.h"


#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/StreamCopier.h"
#include "Poco/Path.h"
#include "Poco/URI.h"
#include "Poco/Exception.h"

/**
 * Backend for the FeatureCollectionDB
 */
class ChronicleDBFeatureCollectionDBBackend : public FeatureCollectionDBBackend {

public:
    ChronicleDBFeatureCollectionDBBackend(const std::string &baseURL);

    virtual ~ChronicleDBFeatureCollectionDBBackend();

    virtual FeatureCollectionDBBackend::datasetid_t
    createPoints(UserDB::User &user, const std::string &dataSetName, const PointCollection &collection);

    virtual std::vector<FeatureCollectionDBBackend::DataSetMetaData> loadDataSetsMetaData(UserDB::User &user) {}
    virtual FeatureCollectionDBBackend::DataSetMetaData loadDataSetMetaData(const UserDB::User &owner, const std::string &dataSetName) {return FeatureCollectionDBBackend::DataSetMetaData();}
    virtual DataSetMetaData loadDataSetMetaData(datasetid_t dataSetId) {}
    virtual FeatureCollectionDBBackend::datasetid_t createLines(UserDB::User &user, const std::string &dataSetName, const LineCollection &collection) {}
    virtual FeatureCollectionDBBackend::datasetid_t createPolygons(UserDB::User &user, const std::string &dataSetName, const PolygonCollection &collection) {}
    virtual std::unique_ptr<PointCollection> loadPoints(const UserDB::User &owner, const std::string& dataSetName, const QueryRectangle &qrect) {}
    virtual std::unique_ptr<LineCollection> loadLines(const UserDB::User &owner, const std::string& dataSetName, const QueryRectangle &qrect) {}
    virtual std::unique_ptr<PolygonCollection> loadPolygons(const UserDB::User &owner, const std::string& dataSetName, const QueryRectangle &qrect) {}

private:
    virtual FeatureCollectionDBBackend::datasetid_t createFeatureCollection(UserDB::User &user, const std::string &dataSetName, const SimpleFeatureCollection &collection, const Query::ResultType &type) {}
    std::string convertPointCollectionToCSV(const PointCollection &collection) const;

    Json::Value
    createPointCollectionImportRequestBody(const std::string &dataSetName, const PointCollection &collection) const;

    Json::Value getSchemaFromPointCollection(const PointCollection &collection) const;

    Json::Value createSchemaColumn(const std::string &name, const std::string &type, bool withIndex) const;

    std::string baseURL;
    Poco::Net::HTTPClientSession *session;
};

REGISTER_FEATURECOLLECTIONDB_BACKEND(ChronicleDBFeatureCollectionDBBackend, "chronicledb");

ChronicleDBFeatureCollectionDBBackend::ChronicleDBFeatureCollectionDBBackend(const std::string &baseURL) : baseURL(
        baseURL) {
    Poco::URI uri(baseURL);
    session = new Poco::Net::HTTPClientSession(uri.getHost(), uri.getPort());

}

ChronicleDBFeatureCollectionDBBackend::~ChronicleDBFeatureCollectionDBBackend() {
    delete session;
}

FeatureCollectionDBBackend::datasetid_t
ChronicleDBFeatureCollectionDBBackend::createPoints(UserDB::User &user, const std::string &dataSetName,
                                                    const PointCollection &collection) {

    Json::Value bodyJSON = createPointCollectionImportRequestBody(dataSetName, collection);
    std::string body = bodyJSON.toStyledString();

    std::string path = "/vat/import"; // TODO set url

    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST, path, Poco::Net::HTTPRequest::HTTP_1_1);
    request.setContentType("application/json");

    request.setContentLength(body.length());

    std::cout << body;

    std::ostream &os = session->sendRequest(request);
    os << body;

    Poco::Net::HTTPResponse response;

    std::cout << response.getStatus() << " " << response.getReason() << std::endl;

    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK) {
        std::istream &is = session->receiveResponse(response);
        std::stringstream ss;
        Poco::StreamCopier::copyStream(is, ss);
        throw ExporterException("failed to export to chronicledb\n" + response.getReason() + "\n" + ss.str());
    }

    return 0;
}

std::string
ChronicleDBFeatureCollectionDBBackend::convertPointCollectionToCSV(const PointCollection &collection) const {
    if (!collection.hasTime()) {
        throw AttributeException("need a timestamp for chronicledb");
    }
    if (collection.stref.timetype != TIMETYPE_UNIX) {
        throw TimeParseException("only unix timestamp is allowed :(");
    }

    std::ostringstream csv;
    csv << std::fixed;

    auto string_keys = collection.feature_attributes.getTextualKeys();
    auto value_keys = collection.feature_attributes.getNumericKeys();

    bool isSimpleCollection = collection.isSimple();

    //header
    if (!isSimpleCollection) {
        csv << "feature,";
    }
    csv << "lon,lat,timeStart,timeEnd";

    for (auto &key : string_keys) {
        csv << "," << key;
    }
    for (auto &key : value_keys) {
        csv << "," << key;
    }
    csv << std::endl;
    // actual data

    for (auto feature : collection) {
        for (auto &c : feature) {
            if (!isSimpleCollection) {
                csv << (size_t) feature << ",";
            }
            csv << c.x << "," << c.y;
            csv << "," << static_cast<long>(collection.time[feature].t1);
            csv << "," << static_cast<long>(collection.time[feature].t2);

            for (auto &key : string_keys) {
                csv << "," << collection.feature_attributes.textual(key).get(feature);
            }
            for (auto &key : value_keys) {
                csv << "," << collection.feature_attributes.numeric(key).get(feature);
            }
            csv << std::endl;
        }
    }
    return csv.str();
}

Json::Value ChronicleDBFeatureCollectionDBBackend::createSchemaColumn(const std::string &name, const std::string &type,
                                                                      bool withIndex) const {
    Json::Value column(Json::objectValue);
    column["name"] = name;
    column["type"] = type;

    if (withIndex) {
        Json::Value properties(Json::objectValue);
        properties["index"] = "true";
        column["properties"] = properties;
    }
    return column;
}

Json::Value
ChronicleDBFeatureCollectionDBBackend::getSchemaFromPointCollection(const PointCollection &collection) const {
    Json::Value schema(Json::arrayValue);
    // add timestamp column
    schema.append(createSchemaColumn("timeStart", "LONG", false));
    // add latitude column
    schema.append(createSchemaColumn("lat", "DOUBLE", true));
    // add longitude column
    schema.append(createSchemaColumn("lon", "DOUBLE", true));
    // add textual attributes
    auto string_keys = collection.feature_attributes.getTextualKeys();
    for (auto &key : string_keys) {
        schema.append(createSchemaColumn(key, "STRING", false));
    }
    // add numeric attributes
    auto value_keys = collection.feature_attributes.getNumericKeys();
    for (auto &key : value_keys) {
        schema.append(createSchemaColumn(key, "DOUBLE", false));
    }

    return schema;
}

Json::Value
ChronicleDBFeatureCollectionDBBackend::createPointCollectionImportRequestBody(const std::string &dataSetName,
                                                                              const PointCollection &collection) const {
    Json::Value body(Json::objectValue);
    // setting up mandatory metadata
    body["path"] = "";
    body["separator"] = ",";
    body["timestampColumn"] = "timeStart";
    body["timestampFormat"] = "UNIX_S";
    body["streamName"] = dataSetName;
    body["schema"] = getSchemaFromPointCollection(collection);
    // setting up actual data
    body["csvdata"] = convertPointCollectionToCSV(collection);

    return body;
}


