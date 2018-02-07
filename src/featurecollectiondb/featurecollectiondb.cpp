
#include "featurecollectiondb.h"
#include "featurecollectiondbbackend.h"
#include "util/configuration.h"
#include "util/log.h"

#include <unordered_map>
#include <cstring>



/*
 * FeatureCollectionDB backend registration
 */
static std::unique_ptr<FeatureCollectionDBBackend> featurecollectiondb_backend;

typedef std::unique_ptr<FeatureCollectionDBBackend> (*BackendConstructor)(const std::string &identifier);

static std::unordered_map< std::string, BackendConstructor > *getRegisteredConstructorsMap() {
	static std::unordered_map< std::string, BackendConstructor > registered_constructors;
	return &registered_constructors;
}

FeatureCollectionDBBackendRegistration::FeatureCollectionDBBackendRegistration(const char *name, BackendConstructor constructor) {
	auto map = getRegisteredConstructorsMap();
	(*map)[std::string(name)] = constructor;
}


/*
 * Initialization
 */
void FeatureCollectionDB::initFromConfiguration() {
	std::string backend;
	try {
		backend = Configuration::get("featurecollectiondb.backend");
	} catch (const ArgumentException &e) {
		Log::info("No configuration found for key featurecollectiondb.backend. Leave FeatureCollectionDB uninitialized.");
		return;
	}
	auto location = Configuration::get("featurecollectiondb." + backend + ".location");
	init(backend, location);
}

void FeatureCollectionDB::init(const std::string &backend, const std::string &location) {
	if (featurecollectiondb_backend != nullptr)
		throw MustNotHappenException("FeatureCollectionDB::init() was called multiple times");

	auto map = getRegisteredConstructorsMap();
	if (map->count(backend) != 1)
		throw ArgumentException(concat("Unknown featurecollectiondb backend: ", backend));


	auto constructor = map->at(backend);
	featurecollectiondb_backend = constructor(location);
}

void FeatureCollectionDB::shutdown() {
	featurecollectiondb_backend = nullptr;
}


std::vector<FeatureCollectionDB::DataSetMetaData> FeatureCollectionDB::loadDataSets(UserDB::User& user) {
	std::vector<FeatureCollectionDB::DataSetMetaData> dataSets;

	// resolve accessible data sets
	// TODO: implement a getPermissions(prefix) method for users to avoid scanning through ALL permissions
	for(const std::string &permission : user.all_permissions.set) {
		if(permission.find("data.featurecollectiondb_source.") == 0){
			size_t dataSetId = std::stoi(permission.substr(strlen("data.featurecollectiondb_source.")));
			try {
				auto dataSet = featurecollectiondb_backend->loadDataSetMetaData(dataSetId);
				dataSets.push_back(dataSet);
			} catch (const std::exception& e) {
				fprintf(stderr, "FeatureCollectionDB: Could not load dataset with id %d", dataSetId);
			}
		}
	}

	return dataSets;
}

FeatureCollectionDB::DataSetMetaData FeatureCollectionDB::loadDataSet(const std::string &owner, const std::string &dataSetName) {
	auto user = UserDB::loadUser(owner);
	return featurecollectiondb_backend->loadDataSetMetaData(*user, dataSetName);
}

std::unique_ptr<PointCollection> FeatureCollectionDB::loadPoints(const std::string &owner, const std::string &dataSetName, const QueryRectangle &qrect) {
	auto user = UserDB::loadUser(owner);
	return featurecollectiondb_backend->loadPoints(*user, dataSetName, qrect);
}

std::unique_ptr<LineCollection> FeatureCollectionDB::loadLines(const std::string &owner, const std::string &dataSetName, const QueryRectangle &qrect) {
	auto user = UserDB::loadUser(owner);
	return featurecollectiondb_backend->loadLines(*user, dataSetName, qrect);
}

std::unique_ptr<PolygonCollection> FeatureCollectionDB::loadPolygons(const std::string &owner, const std::string &dataSetName, const QueryRectangle &qrect) {
	auto user = UserDB::loadUser(owner);
	return featurecollectiondb_backend->loadPolygons(*user, dataSetName, qrect);
}


FeatureCollectionDB::DataSetMetaData FeatureCollectionDB::createPoints(UserDB::User &user, const std::string &dataSetName, const PointCollection &collection) {
	datasetid_t datasetId = featurecollectiondb_backend->createPoints(user, dataSetName, collection);
	return featurecollectiondb_backend->loadDataSetMetaData(user, dataSetName);
}

FeatureCollectionDB::DataSetMetaData FeatureCollectionDB::createLines(UserDB::User &user, const std::string &dataSetName, const LineCollection &collection) {
	datasetid_t datasetId = featurecollectiondb_backend->createLines(user, dataSetName, collection);
	return featurecollectiondb_backend->loadDataSetMetaData(user, dataSetName);
}

FeatureCollectionDB::DataSetMetaData FeatureCollectionDB::createPolygons(UserDB::User &user, const std::string &dataSetName, const PolygonCollection &collection) {
	datasetid_t datasetId = featurecollectiondb_backend->createPolygons(user, dataSetName, collection);
	return featurecollectiondb_backend->loadDataSetMetaData(user, dataSetName);
}

