
#ifndef FEATURECOLLECTIONDB_FEATURECOLLECTIONDBBACKEND_CPP_
#define FEATURECOLLECTIONDB_FEATURECOLLECTIONDBBACKEND_CPP_

#include "featurecollectiondb/featurecollectiondb.h"
#include "datatypes/simplefeaturecollection.h"
#include "processing/query.h"
#include "util/make_unique.h"

#include <memory>


/**
 * Backend for the FeatureCollectionDB
 */
class FeatureCollectionDBBackend {
	friend class FeatureCollectionDB;

protected:
	using datasetid_t = FeatureCollectionDB::datasetid_t;
	using DataSetMetaData = FeatureCollectionDB::DataSetMetaData;

public:

	virtual ~FeatureCollectionDBBackend() {};


	/**
	 * load meta data of all data sets of a user
	 */
	virtual std::vector<DataSetMetaData> loadDataSetsMetaData(UserDB::User &user) = 0;

	/**
	 * load meta data for a given data set
	 */
	virtual DataSetMetaData loadDataSetMetaData(const UserDB::User &owner, const std::string &dataSetName) = 0;

	/**
	 * load meta data for a given data set
	 */
	virtual DataSetMetaData loadDataSetMetaData(datasetid_t dataSetId) = 0;

	/**
	 * create a data set for given user
	 */
	virtual datasetid_t createPoints(UserDB::User &user, const std::string &dataSetName, const PointCollection &collection) = 0;

	/**
	 * create a data set for given user
	 */
	virtual datasetid_t createLines(UserDB::User &user, const std::string &dataSetName, const LineCollection &collection) = 0;

	/**
	 * create a data set for given user
	 */
	virtual datasetid_t createPolygons(UserDB::User &user, const std::string &dataSetName, const PolygonCollection &collection) = 0;

	/**
	 * load the data of the given data set
	 */
	virtual std::unique_ptr<PointCollection> loadPoints(const UserDB::User &owner, const std::string &dataSetName, const QueryRectangle &qrect) = 0;

	/**
	 * load the data of the given data set
	 */
	virtual std::unique_ptr<LineCollection> loadLines(const UserDB::User &owner, const std::string &dataSetName, const QueryRectangle &qrect) = 0;

	/**
	 * load the data of the given data set
	 */
	virtual std::unique_ptr<PolygonCollection> loadPolygons(const UserDB::User &owner, const std::string &dataSetName, const QueryRectangle &qrect) = 0;

};

class FeatureCollectionDBBackendRegistration {
	public:
	FeatureCollectionDBBackendRegistration(const char *name, std::unique_ptr<FeatureCollectionDBBackend> (*constructor)(const std::string &));
};

#define REGISTER_FEATURECOLLECTIONDB_BACKEND(classname, name) static std::unique_ptr<FeatureCollectionDBBackend> create##classname(const std::string &location) { return make_unique<classname>(location); } static FeatureCollectionDBBackendRegistration register_##classname(name, create##classname)


#endif /* FEATURECOLLECTIONDB_FEATURECOLLECTIONDBBACKEND_CPP_ */
