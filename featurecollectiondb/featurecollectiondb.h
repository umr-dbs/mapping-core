#ifndef FEATURECOLLECTIONDB_FEATURECOLLECTIONDB_H_
#define FEATURECOLLECTIONDB_FEATURECOLLECTIONDB_H_

#include "userdb/userdb.h"
#include "datatypes/pointcollection.h"
#include "datatypes/linecollection.h"
#include "datatypes/polygoncollection.h"
#include "processing/query.h"

#include <memory>
#include <vector>
#include <map>

/**
 * This class allows storing SimpleFeatureCollections for users in a backend
 */
class FeatureCollectionDB {
protected:
	using datasetid_t = int64_t;
	friend class FeatureCollectionDBBackend;

public:

	static void initFromConfiguration();
	static void init(const std::string &backend, const std::string &location);
	static void shutdown();

	// TODO: owner of the data set?
	struct DataSetMetaData {
		size_t dataSetId;
		std::string owner;
		std::string dataSetName;
		Query::ResultType resultType;
		std::map<std::string, Unit> numeric_attributes;
		std::map<std::string, Unit> textual_attributes;
		bool hasTime;

	};

	/**
	 * Load Info of all data sets visible to the user
	 */
	static std::vector<DataSetMetaData> loadDataSets(UserDB::User& user);

	/**
	 * Load Info of given data set
	 */
	static DataSetMetaData loadDataSet(const std::string &owner, const std::string &dataSetName);

	/**
	 * load a feature collection
	 */
	static std::unique_ptr<PointCollection> loadPoints(const std::string &owner, const std::string &dataSetName, const QueryRectangle &qrect);

	/**
	 * load a feature collection
	 */
	static std::unique_ptr<LineCollection> loadLines(const std::string &owner, const std::string &dataSetName, const QueryRectangle &qrect);

	/**
	 * load a feature collection
	 */
	static std::unique_ptr<PolygonCollection> loadPolygons(const std::string &owner, const std::string &dataSetName, const QueryRectangle &qrect);


	/**
	 * save a feature collection
	 */
	static DataSetMetaData createPoints(UserDB::User &user, const std::string &dataSetName, const PointCollection &collection);

	/**
	 * save a feature collection
	 */
	static DataSetMetaData createLines(UserDB::User &user, const std::string &dataSetName, const LineCollection &collection);

	/**
	 * save a feature collection
	 */
	static DataSetMetaData createPolygons(UserDB::User &user, const std::string &dataSetName, const PolygonCollection &collection);

};

#endif /* FEATURECOLLECTIONDB_FEATURECOLLECTIONDB_H_ */
