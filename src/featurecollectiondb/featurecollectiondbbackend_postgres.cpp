#include "featurecollectiondb/featurecollectiondbbackend.h"
#include "datatypes/simplefeaturecollection.h"
#include "processing/query.h"
#include "datatypes/simplefeaturecollections/wkbutil.h"
#include "util/exceptions.h"
#include "util/enumconverter.h"
#include "util/make_unique.h"

#include <memory>
#include <pqxx/pqxx>
#include <regex>
#include <json/json.h>

/**
 * Backend for the FeatureCollectionDB
 */
class PostgresFeatureCollectionDBBackend : public FeatureCollectionDBBackend {

public:
	PostgresFeatureCollectionDBBackend(const std::string &connectionString);

	virtual ~PostgresFeatureCollectionDBBackend();

	virtual std::vector<FeatureCollectionDBBackend::DataSetMetaData> loadDataSetsMetaData(UserDB::User &user);
	virtual FeatureCollectionDBBackend::DataSetMetaData loadDataSetMetaData(const UserDB::User &owner, const std::string &dataSetName);
	virtual DataSetMetaData loadDataSetMetaData(datasetid_t dataSetId);
	virtual FeatureCollectionDBBackend::datasetid_t createPoints(UserDB::User &user, const std::string &dataSetName, const PointCollection &collection);
	virtual FeatureCollectionDBBackend::datasetid_t createLines(UserDB::User &user, const std::string &dataSetName, const LineCollection &collection);
	virtual FeatureCollectionDBBackend::datasetid_t createPolygons(UserDB::User &user, const std::string &dataSetName, const PolygonCollection &collection);
	virtual std::unique_ptr<PointCollection> loadPoints(const UserDB::User &owner, const std::string& dataSetName, const QueryRectangle &qrect);
	virtual std::unique_ptr<LineCollection> loadLines(const UserDB::User &owner, const std::string& dataSetName, const QueryRectangle &qrect);
	virtual std::unique_ptr<PolygonCollection> loadPolygons(const UserDB::User &owner, const std::string& dataSetName, const QueryRectangle &qrect);

private:


	virtual FeatureCollectionDBBackend::datasetid_t createFeatureCollection(UserDB::User &user, const std::string &dataSetName, const SimpleFeatureCollection &collection, const Query::ResultType &type);
	FeatureCollectionDBBackend::datasetid_t createDataSet(pqxx::work &work, const UserDB::User &user, const std::string &dataSetName, const Query::ResultType type, const SimpleFeatureCollection &collection);
	void createDataSetTable(pqxx::work &work, const std::string &tableName, const SimpleFeatureCollection &collection);
	void insertDataIntoTable(pqxx::work &work, const std::string &tableName, const SimpleFeatureCollection &collection);
	FeatureCollectionDBBackend::DataSetMetaData dataSetRowToMetaData(pqxx::result::tuple& row);
	void loadFeatures(SimpleFeatureCollection &collection,const Query::ResultType &type, const UserDB::User &owner, const std::string &dataSetName, const QueryRectangle &qrect);

	std::string connectionString;
};


REGISTER_FEATURECOLLECTIONDB_BACKEND(PostgresFeatureCollectionDBBackend, "postgres");

static thread_local std::unique_ptr<pqxx::connection> _connection;

static pqxx::connection& getConnection(const std::string& connectionString) {
	if(_connection.get() == nullptr) {
		_connection = make_unique<pqxx::connection>(connectionString);
	}

	return *_connection;
}

PostgresFeatureCollectionDBBackend::PostgresFeatureCollectionDBBackend(const std::string &connectionString) : connectionString(connectionString){
	auto& connection = getConnection(connectionString);

	// TODO: global attributes
	// TODO: simple or multi features
	pqxx::work work(connection);
	work.exec("CREATE TABLE IF NOT EXISTS datasets ("
				"datasetid SERIAL PRIMARY KEY,"
				"userid INTEGER,"
				"name text,"
				"type text,"
				"numeric_attributes JSON,"
				"textual_attributes JSON,"
				"has_time boolean,"
				"UNIQUE (userid, name)"
			");");
	work.commit();
}

PostgresFeatureCollectionDBBackend::~PostgresFeatureCollectionDBBackend() {
}



std::vector<FeatureCollectionDBBackend::DataSetMetaData> PostgresFeatureCollectionDBBackend::loadDataSetsMetaData(UserDB::User &user) {
	auto& connection = getConnection(connectionString);

	std::vector<FeatureCollectionDB::DataSetMetaData> dataSets;

	// TODO: resolve all data sets the user has access to (rights)

	connection.prepare("datasets", "SELECT datasetid, userid, name, type, numeric_attributes, textual_attributes, has_time FROM datasets WHERE userid = $1");
	pqxx::work work(connection);

	pqxx::result result = work.prepared("datasets")(user.userid).exec();

	for(size_t i = 0; i < result.size(); ++i) {
		auto row = result[i];

		FeatureCollectionDBBackend::DataSetMetaData metaData = dataSetRowToMetaData(row);

		dataSets.push_back(metaData);
	}
	return dataSets;
}

FeatureCollectionDBBackend::DataSetMetaData PostgresFeatureCollectionDBBackend::dataSetRowToMetaData(pqxx::result::tuple& row) {
	size_t datasetid = row["datasetid"].as<size_t>();
	size_t userid = row["userid"].as<size_t>();

	std::string name = row["name"].as<std::string>();
	std::string type = row["type"].as<std::string>();

	Query::ResultType resultType;
	if(type == "points") {
		resultType = Query::ResultType::POINTS;
	} else if(type == "lines") {
		resultType = Query::ResultType::LINES;
	} else if(type == "polygons") {
		resultType = Query::ResultType::POLYGONS;
	} else {
		throw MustNotHappenException("Invalid type of feature collection", MappingExceptionType::TRANSIENT);
	}

	Json::Value json;
	Json::Reader reader;
	reader.parse(row["numeric_attributes"].as<std::string>().c_str(), json );
	std::map<std::string, Unit> numeric_attributes;
	for(int i = 0; i < json.size(); ++i) {
		numeric_attributes.emplace(json[i]["key"].asString(), json[i]["unit"]);
	}

	reader.parse(row["textual_attributes"].as<std::string>().c_str(), json );
	std::map<std::string, Unit> textual_attributes;
	for(int i = 0; i < json.size(); ++i) {
		textual_attributes.emplace(json[i]["key"].asString(), json[i]["unit"]);
	}

	bool hasTime = row["has_time"].as<bool>();

	auto user = UserDB::loadUser(userid);

	return DataSetMetaData{datasetid, user->getUsername(), name, resultType, numeric_attributes, textual_attributes, hasTime};
}

FeatureCollectionDBBackend::DataSetMetaData PostgresFeatureCollectionDBBackend::loadDataSetMetaData(const UserDB::User &owner, const std::string &dataSetName) {
	auto& connection = getConnection(connectionString);

	// TODO avoid preparing statements redundantly
	connection.prepare("select_dataset_by_owner_name", "SELECT datasetid, userid, name, type, numeric_attributes, textual_attributes, has_time FROM datasets WHERE userid = $1 AND name = $2");
	pqxx::work work(connection);

	pqxx::result result = work.prepared("select_dataset_by_owner_name")(owner.userid)(dataSetName).exec();

	if(result.size() < 1) {
		throw ArgumentException("PostgresFeatureCollectionDB: data set with given owner and name does not exist");
	}

	auto row = result[0];

	return dataSetRowToMetaData(row);
}

FeatureCollectionDBBackend::DataSetMetaData PostgresFeatureCollectionDBBackend::loadDataSetMetaData(datasetid_t dataSetId) {
	auto& connection = getConnection(connectionString);

	connection.prepare("select_dataset_by_id", "SELECT datasetid, userid, name, type, numeric_attributes, textual_attributes, has_time FROM datasets WHERE datasetid = $1");
	pqxx::work work(connection);

	pqxx::result result = work.prepared("select_dataset_by_id")(dataSetId).exec();

	if(result.size() < 1) {
		throw ArgumentException("PostgresFeatureCollectionDB: data set with given id does not exist");
	}

	auto row = result[0];

	return dataSetRowToMetaData(row);
}


FeatureCollectionDBBackend::datasetid_t PostgresFeatureCollectionDBBackend::createDataSet(pqxx::work &work, const UserDB::User &user, const std::string &dataSetName, const Query::ResultType type, const SimpleFeatureCollection &collection) {
	auto& connection = getConnection(connectionString);

	connection.prepare("insert_dataset", "INSERT INTO datasets (userid, name, type, numeric_attributes, textual_attributes, has_time) VALUES ($1, $2, $3, $4, $5, $6) RETURNING datasetid");

	Json::Value numeric_attributes(Json::arrayValue);
	for(auto& attribute : collection.feature_attributes.getNumericKeys()) {
		Json::Value jsonAttribute(Json::objectValue);
		jsonAttribute["key"] = attribute;
		jsonAttribute["unit"] = collection.feature_attributes.numeric(attribute).unit.toJsonObject();

		numeric_attributes.append(jsonAttribute);
	}

	Json::Value textual_attributes(Json::arrayValue);
	for(auto& attribute : collection.feature_attributes.getTextualKeys()) {
		Json::Value jsonAttribute(Json::objectValue);
		jsonAttribute["key"] = attribute;
		jsonAttribute["unit"] = collection.feature_attributes.textual(attribute).unit.toJsonObject();

		textual_attributes.append(jsonAttribute);
	}

	Json::FastWriter writer;

	std::string typeString;
	if(type == Query::ResultType::POINTS) {
		typeString = "points";
	} else if(type == Query::ResultType::LINES) {
		typeString = "lines";
	} else if(type == Query::ResultType::POLYGONS) {
		typeString = "polygons";
	} else {
		throw ArgumentException("PostgresFeatureCollectionDBBackend: unknown result type", MappingExceptionType::PERMANENT);
	}

	pqxx::result result = work.prepared("insert_dataset")(user.userid)(dataSetName)(typeString)(writer.write(numeric_attributes))(writer.write(textual_attributes))(collection.hasTime()).exec();

	return result[0][0].as<size_t>();
}

void PostgresFeatureCollectionDBBackend::createDataSetTable(pqxx::work &work, const std::string &tableName, const SimpleFeatureCollection &collection) {
	std::stringstream query;

	query << "CREATE TABLE " << tableName << " (";

	int i = 0;
	for(auto &attribute : collection.feature_attributes.getNumericKeys()) {
		query << "numeric_" << i++ << " double precision,";
	}

	i = 0;
	for(auto &attribute : collection.feature_attributes.getTextualKeys()) {
		query << "textual_" << i++ << " text,";
	}

	if(collection.hasTime()) {
		// TODO: time as timestamp??
		query << "time_start double precision,";
		query << "time_end double precision,";
	}

	query << "geom geometry, feature_index integer primary key";

	query << ");";

	work.exec(query.str());
}

void PostgresFeatureCollectionDBBackend::insertDataIntoTable(pqxx::work &work, const std::string &tableName, const SimpleFeatureCollection &collection) {
	auto& connection = getConnection(connectionString);

	// build the insert statement
	std::stringstream query;
	std::stringstream placeholders;

	query << "INSERT INTO " << tableName << " (";

	size_t parameterId = 1;

	int i = 0;
	for(auto &attribute : collection.feature_attributes.getNumericKeys()) {
		query << "numeric_" << i++ << ",";
		placeholders << "$" << parameterId++ << ",";
	}

	i = 0;
	for(auto &attribute : collection.feature_attributes.getTextualKeys()) {
		query << "textual_" << i++ << ",";
		placeholders << "$" << parameterId++ << ",";
	}

	bool hasTime = collection.hasTime();
	if(hasTime) {
		query << "time_start,";
		query << "time_end,";
		placeholders << "$" << parameterId++ << ",$" <<parameterId++ << ",";
	}

	query << "geom, feature_index";
	// TODO: SRID
	placeholders << "ST_GeomFromText($" << parameterId++ << "),$" << parameterId++;

	query << ") VALUES (" << placeholders.str() << ")";

	std::string prepare = "insert" + tableName;
	connection.unprepare(prepare);
	connection.prepare(prepare, query.str());

	// perform insertions
	for(size_t i = 0; i < collection.getFeatureCount(); ++i) {
		auto prep = work.prepared(prepare);

		for(auto &attribute : collection.feature_attributes.getNumericKeys()) {
			prep(collection.feature_attributes.numeric(attribute).get(i));
		}

		for(auto &attribute : collection.feature_attributes.getTextualKeys()) {
			prep(collection.feature_attributes.textual(attribute).get(i));
		}

		if(hasTime) {
			prep(collection.time[i].t1);
			prep(collection.time[i].t2);
		}

		prep(collection.featureToWKT(i));
		prep(i);
		prep.exec();
	}
}


FeatureCollectionDBBackend::datasetid_t PostgresFeatureCollectionDBBackend::createFeatureCollection(UserDB::User &user, const std::string &dataSetName, const SimpleFeatureCollection &collection, const Query::ResultType &type) {
	auto& connection = getConnection(connectionString);

	pqxx::work work(connection);

	size_t dataSetId = createDataSet(work, user, dataSetName, type, collection);

	std::string tableName = concat("dataset_", dataSetId);

	createDataSetTable(work, tableName, collection);

	insertDataIntoTable(work, tableName, collection);

	// TODO: create indexes

	work.commit();
	return dataSetId;
}



FeatureCollectionDBBackend::datasetid_t PostgresFeatureCollectionDBBackend::createPoints(UserDB::User &user, const std::string &dataSetName, const PointCollection &collection) {
	return createFeatureCollection(user, dataSetName, collection, Query::ResultType::POINTS);
}

FeatureCollectionDBBackend::datasetid_t PostgresFeatureCollectionDBBackend::createLines(UserDB::User &user, const std::string &dataSetName, const LineCollection &collection) {
	return createFeatureCollection(user, dataSetName, collection, Query::ResultType::LINES);
}

FeatureCollectionDBBackend::datasetid_t PostgresFeatureCollectionDBBackend::createPolygons(UserDB::User &user, const std::string &dataSetName, const PolygonCollection &collection) {
	return createFeatureCollection(user, dataSetName, collection, Query::ResultType::POLYGONS);
}

void PostgresFeatureCollectionDBBackend::loadFeatures(SimpleFeatureCollection &collection, const Query::ResultType &type, const UserDB::User &owner, const std::string &dataSetName, const QueryRectangle &qrect) {
	auto& connection = getConnection(connectionString);

	//get meta data
	auto metaData = loadDataSetMetaData(owner, dataSetName);

	// build query and prepare collection
	std::stringstream query;

	query << "SELECT ";

	int i = 0;
	for(auto &attribute : metaData.numeric_attributes) {
		query << "numeric_" << i++ << ",";
		collection.feature_attributes.addNumericAttribute(attribute.first, attribute.second);
	}

	i = 0;
	for(auto &attribute : metaData.textual_attributes) {
		query << "textual_" << i++ << ",";
		collection.feature_attributes.addTextualAttribute(attribute.first, attribute.second);
	}

	if(metaData.hasTime) {
		query << "time_start,";
		query << "time_end,";
	}

	query << "ST_AsEWKT(geom) geom";

	// TODO get tableName from registry?
	query << " FROM dataset_" << metaData.dataSetId;
	query << " WHERE ST_INTERSECTS(geom, ST_MakeEnvelope($1, $2, $3, $4))";
	if(metaData.hasTime) {
		query << " AND ((to_timestamp(time_start), to_timestamp(time_end)) OVERLAPS (to_timestamp($5), to_timestamp($6)))";
	}
	query << " ORDER BY feature_index ASC";


	pqxx::work work(connection);

	std::string prepare = "select_" + dataSetName;
	connection.unprepare(prepare);
	connection.prepare(prepare, query.str());

	auto prep = work.prepared(prepare);
	prep(qrect.x1)(qrect.y1)(qrect.x2)(qrect.y2);
	if(metaData.hasTime) {
		prep(qrect.t1)(qrect.t2);
	}

	pqxx::result result = prep.exec();

	// TODO: global attributes

	for(size_t i = 0; i < result.size(); ++i) {
		auto row = result[i];

		// geom
		if (type == Query::ResultType::POINTS) {
			WKBUtil::addFeatureToCollection(dynamic_cast<PointCollection&>(collection), row["geom"].as<std::string>());
		} else if (type == Query::ResultType::LINES) {
			WKBUtil::addFeatureToCollection(dynamic_cast<LineCollection&>(collection), row["geom"].as<std::string>());
		} else if (type == Query::ResultType::POLYGONS) {
			WKBUtil::addFeatureToCollection(dynamic_cast<PolygonCollection&>(collection), row["geom"].as<std::string>());
		} else {
			throw ArgumentException("PostgresFeatureCollectionDBBackend: Invalid type of feature collection", MappingExceptionType::PERMANENT);
		}

		// attributes
		int a = 0;
		for(auto &attribute : metaData.numeric_attributes) {
			collection.feature_attributes.numeric(attribute.first).set(i, row[a++].as<double>());
		}

		a = 0;
		int offset = metaData.numeric_attributes.size();
		for(auto &attribute : metaData.textual_attributes) {
			collection.feature_attributes.textual(attribute.first).set(i, row[offset + (a++)].as<std::string>());
		}

		// time
		if(metaData.hasTime) {
			collection.time.push_back(TimeInterval(row["time_start"].as<double>(), row["time_end"].as<double>()));
		}
	}
}


std::unique_ptr<PointCollection> PostgresFeatureCollectionDBBackend::loadPoints(const UserDB::User &owner, const std::string &dataSetName, const QueryRectangle &qrect) {
	auto points = make_unique<PointCollection>(qrect);
	loadFeatures(*points, Query::ResultType::POINTS, owner, dataSetName, qrect);
	return points;
}

std::unique_ptr<LineCollection> PostgresFeatureCollectionDBBackend::loadLines(const UserDB::User &owner, const std::string &dataSetName, const QueryRectangle &qrect) {
	auto lines = make_unique<LineCollection>(qrect);
	loadFeatures(*lines, Query::ResultType::LINES,  owner, dataSetName, qrect);
	return lines;
}

std::unique_ptr<PolygonCollection> PostgresFeatureCollectionDBBackend::loadPolygons(const UserDB::User  &owner, const std::string &dataSetName, const QueryRectangle &qrect) {
	auto polygons = make_unique<PolygonCollection>(qrect);
	loadFeatures(*polygons, Query::ResultType::POLYGONS, owner, dataSetName, qrect);
	return polygons;
}
