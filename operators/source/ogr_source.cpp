#include <gdal/ogrsf_frmts.h>
#include "operators/operator.h"
#include "datatypes/polygoncollection.h"
#include "datatypes/linecollection.h"
#include "datatypes/pointcollection.h"
#include "datatypes/simplefeaturecollection.h"

#include "util/enumconverter.h"
#include "util/gdal_timesnap.h"
#include "util/timeparser.h"
#include "util/exceptions.h"
#include "util/make_unique.h"
#include "util/configuration.h"
#include <json/json.h>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <memory>

enum class AttributeType
{
	TEXTUAL,
	NUMERIC,
	TIME
};

enum class ErrorHandling
{
	ABORT,
	SKIP,
	KEEP
};

const std::vector< std::pair<ErrorHandling, std::string> > ErrorHandlingMap {
	std::make_pair(ErrorHandling::ABORT, "abort"),
	std::make_pair(ErrorHandling::SKIP, "skip"),
	std::make_pair(ErrorHandling::KEEP, "keep")
};

static EnumConverter<ErrorHandling>ErrorHandlingConverter(ErrorHandlingMap);

enum class TimeSpecification {
	NONE,
	START,
	START_END,
	START_DURATION
};

const std::vector< std::pair<TimeSpecification, std::string> > TimeSpecificationMap = {
	std::make_pair(TimeSpecification::NONE, "none"),
	std::make_pair(TimeSpecification::START, "start"),
	std::make_pair(TimeSpecification::START_END, "start+end"),
	std::make_pair(TimeSpecification::START_DURATION, "start+duration")
};

static EnumConverter<TimeSpecification> TimeSpecificationConverter(TimeSpecificationMap);

/*
  * Parameters:
 * - filename: path to the input file
 *
 * - field_separator: the delimiter
 * - geometry_specification: the type in the geometry column(s)
 *   - "xy": two columns for the 2 spatial dimensions
 *   - "wkt": a single column containing the feature geometry as well-known-text
 * - time: the type of the time column(s)
 *   - "none": no time information is mapped
 *   - "start": only start information is mapped. duration has to specified in the duration attribute
 *   - "start+end": start and end information is mapped
 *   - "start+duration": start and duration information is mapped
 * -  duration: the duration of the time validity for all features in the file
 * - time1_format: a json object mapping a column to the start time
 *   - format: define the format of the column
 *     - "custom": define a custom format in the attribute "custom_format"
 *     - "seconds": time column is numeric and contains seconds as UNIX timestamp
 *     - "dmyhm": %d-%B-%Y  %H:%M
 *     - "iso": time column contains string with ISO8601
 * - time2_format: a json object mapping a columns to the end time (cf. time1_format)
 * - columns: a json object mapping the columns to data, time, space. Columns that are not listed are skipped when parsin.
 *   - x: the name of the column containing the x coordinate (or the wkt string)
 *   - y: the name of the column containing the y coordinate
 *   - time1: the name of the first time column
 *   - time2: the name of the second time column
 *   - numeric: an array of column names containing numeric values
 *   - textual: an array of column names containing alpha-numeric values
 * - on_error: specify the type of error handling
 *   - "skip"
 *   - "abort"
 *   - "keep"
 * - provenance: specify the provenance of a file as an array of json object containing
 *   - citation
 *   - license
 *   - uri
 *
 */
class OGRSourceOperator : public GenericOperator {
	public:
		OGRSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);
		virtual ~OGRSourceOperator();

		virtual std::unique_ptr<PointCollection> getPointCollection(const QueryRectangle &rect, const QueryTools &tools);
		virtual std::unique_ptr<LineCollection> getLineCollection(const QueryRectangle &rect, const QueryTools &tools);
		virtual std::unique_ptr<PolygonCollection> getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools);

	protected:
		void writeSemanticParameters(std::ostringstream& stream);
		virtual void getProvenance(ProvenanceCollection &pc);
		
	private:		
		std::string filename;
		GDALDataset *dataset;
		std::string *attributeNames;
		std::unordered_map<std::string, AttributeType> wantedAttributes;
		std::string time1Name;
		std::string time2Name;
		std::unique_ptr<TimeParser> time1Parser;
		std::unique_ptr<TimeParser> time2Parser;
		ErrorHandling errorHandling;
		TimeSpecification timeSpecification;
		Provenance provenance;
		Json::Value parameter;

		OGRLayer* loadLayer(const QueryRectangle &rect);
		void readRingToPolygonCollection(const OGRLinearRing *ring, std::unique_ptr<PolygonCollection> &collection);
		void readLineStringToLineCollection(const OGRLineString *line, std::unique_ptr<LineCollection> &collection);
		void createAttributeArrays(OGRFeatureDefn *attributeDefn, AttributeArrays &collection);
		bool readAttributesIntoCollection(AttributeArrays &attributeArrays, OGRFeatureDefn *attributeDefn, OGRFeature *feature, int featureIndex);
		void readAnyCollection(SimpleFeatureCollection *featureCollection, OGRLayer *layer, std::function<bool(OGRGeometry *, OGRFeature *, int)> addFeature);
		bool hasSuffix(const std::string &str, const std::string &suffix);		
		void close();
};

OGRSourceOperator::OGRSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params) : GenericOperator(sourcecounts, sources) {
	assumeSources(0);
	parameter = params;
	filename = params.get("filename", "").asString();
	errorHandling = ErrorHandlingConverter.from_json(params, "on_error");
	Json::Value provenanceInfo = params["provenance"];
	if (provenanceInfo.isObject()) {
		provenance = Provenance(provenanceInfo.get("citation", "").asString(),
				provenanceInfo.get("license", "").asString(),
				provenanceInfo.get("uri", "").asString(), "data.ogr_source." + filename);
	}

	auto columns = params.get("columns", Json::Value(Json::ValueType::objectValue));
	auto textual = columns.get("textual", Json::Value(Json::ValueType::arrayValue));
    for (auto &name : textual)
    	wantedAttributes[name.asString()] = AttributeType::TEXTUAL;
    
    auto numeric = columns.get("numeric", Json::Value(Json::ValueType::arrayValue));
    for (auto &name : numeric)
    	wantedAttributes[name.asString()] = AttributeType::NUMERIC;

    timeSpecification = TimeSpecificationConverter.from_json(params, "time");
    
}

OGRSourceOperator::~OGRSourceOperator()
{
	close();
}

REGISTER_OPERATOR(OGRSourceOperator, "ogr_source");

void OGRSourceOperator::writeSemanticParameters(std::ostringstream& stream) {
	/*Json::Value params = csvSourceUtil->getParameters();

	params["filename"] = filename;

	Json::Value provenanceInfo;
	provenanceInfo["citation"] = provenance.citation;
	provenanceInfo["license"] = provenance.license;
	provenanceInfo["uri"] = provenance.uri;
	params["provenance"] = provenanceInfo;

	Json::FastWriter writer;
	stream << writer.write(params);
	*/
}

void OGRSourceOperator::getProvenance(ProvenanceCollection &pc)
{	
	pc.add(provenance);
}

OGRLayer* OGRSourceOperator::loadLayer(const QueryRectangle &rect)
{		
	GDALAllRegister();

	const char * const *oo = NULL;	
	bool isCsv = hasSuffix(filename, ".csv");

	// does not work!
	// GDALOpenEx expects a const char * const *. hardcoding the values on array creation 
	// works as expected but putting the column_x/y values in it does not work
	// also putting not concatenated const char * does not work
	if(isCsv){
		auto columns = parameter.get("columns", Json::Value(Json::ValueType::objectValue));
		std::string column_x = columns.get("x", "x").asString();
		
		if(columns.isMember("y"))
		{	
			std::string column_y 	= columns.get("y", "y").asString();
			std::string optX 		= "X_POSSIBLE_NAMES=X" + column_x;
			std::string optY 		= "Y_POSSIBLE_NAMES=Y" + column_y;
			std::string optCol 		= "KEEP_GEOM_COLUMNS=NO";
			
			//const char * const str[] = { optX.c_str(), optY.c_str(), optCol.c_str() };	//does not work
			const char * const strs[] = { "X_POSSIBLE_NAMES=X", "Y_POSSIBLE_NAMES=Y", "KEEP_GEOM_COLUMNS=NO" }; //hardcoded does work
			oo = strs;
		} 
		else 	
		{
			std::string opt = "GEOM_POSSIBLE_NAMES=" + column_x;			
			const char * const strs[] = { opt.c_str(), "KEEP_GEOM_COLUMNS=NO", NULL };	// does not work
			oo = strs;			
		}		
	}

	dataset = (GDALDataset*)GDALOpenEx(filename.c_str(), GDAL_OF_VECTOR, NULL, oo, NULL);

	if(dataset == NULL){
		close();
		throw OperatorException("Can not load dataset");
	}

	if(dataset->GetLayerCount() < 1){
		close();
		throw OperatorException("No layers in OGR Dataset");
	}

	OGRLayer *layer;
	// for now only take layer 0 in consideration
	layer = dataset->GetLayer(0);

	layer->SetSpatialFilterRect(rect.x1, rect.y1, rect.x2, rect.y2);

	//just in case call suggested by OGR Tutorial
	layer->ResetReading();

	return layer;
}

void OGRSourceOperator::readAnyCollection(SimpleFeatureCollection *collection, OGRLayer *layer, std::function<bool(OGRGeometry *, OGRFeature *, int)> addFeature)
{
	OGRFeatureDefn *attributeDefn = layer->GetLayerDefn();
	createAttributeArrays(attributeDefn, collection->feature_attributes);

	OGRFeature* feature;

	int featureCount = 0;

	while( (feature = layer->GetNextFeature()) != NULL )
	{
		//each feature has potentially multiple geometries
		if(feature->GetGeomFieldCount() > 1){
			close();
			throw OperatorException("OGR Source: at least one OGRFeature has more than one Geometry. For now we don't support this.");
		}
		
		OGRGeometry *geom = feature->GetGeomFieldRef(0);
		bool success = false;
		if(geom != NULL)		
			success = addFeature(geom, feature, featureCount);

		//returns false if attribute could not be written and errorhandling is SKIP: the inserted feature has to be removed
		if(success && !readAttributesIntoCollection(collection->feature_attributes, attributeDefn, feature, featureCount)){
			success = false;
			collection->removeLastFeature();
		}

		if(success)
			featureCount++;					
		else
		{
			switch(errorHandling){
				case ErrorHandling::ABORT:
					close();
					if(geom == NULL)
						throw OperatorException("OGR Source: Invalid dataset, at least one OGRGeometry was NULL.");
					else
						throw OperatorException("OGR Source: Dataset contains not expected FeatureType (Points,Lines,Polygons)");
					break;
				case ErrorHandling::SKIP:
					//removing the last written feature is handled in addFeature because here 
					//we can not know if writing has actually started or if a type missmatch happened.
					break;
				case ErrorHandling::KEEP:
					//there isn't really anything that can be done for KEEP
					break;
			}
		}
			
		OGRFeature::DestroyFeature(feature);	

	}
	close();
}

std::unique_ptr<PointCollection> OGRSourceOperator::getPointCollection(const QueryRectangle &rect, const QueryTools &tools)
{
	auto points = make_unique<PointCollection>(rect);
	OGRLayer *layer = loadLayer(rect);
	OGRFeatureDefn *attributeDefn = layer->GetLayerDefn();

	auto addFeature = [&](OGRGeometry *geom, OGRFeature *feature, int featureCount) -> bool 
	{
		int type = wkbFlatten(geom->getGeometryType());							
		
		if(type == wkbPoint)
		{	
			OGRPoint *point = (OGRPoint *)geom;				
			points->addCoordinate(point->getX(), point->getY());
			points->finishFeature();	
		}				
		else if(type == wkbMultiPoint)
		{
			OGRMultiPoint *mp = (OGRMultiPoint *)geom;
			int num = mp->getNumGeometries();
			for(int k = 0; k < num; k++){
				OGRPoint *point = (OGRPoint *)mp->getGeometryRef(k);
				points->addCoordinate(point->getX(), point->getY());
				points->finishFeature();
			}			
		}
		else 
			return false;

		return true;	
	};

	readAnyCollection(points.get(), layer, addFeature);
	points->validate();
	return points;
}

std::unique_ptr<LineCollection> OGRSourceOperator::getLineCollection(const QueryRectangle &rect, const QueryTools &tools)
{
	auto lines = make_unique<LineCollection>(rect);
	OGRLayer *layer = loadLayer(rect);

	auto addFeature = [&](OGRGeometry *geom, OGRFeature *feature, int featureCount) -> bool 
	{
		int type = wkbFlatten(geom->getGeometryType());
		
		if(type == wkbLineString)
		{
			OGRLineString *ls = (OGRLineString *)geom;
			readLineStringToLineCollection(ls, lines);									
			lines->finishFeature();
		}
		else if(type == wkbMultiLineString)
		{
			OGRMultiLineString *mls = (OGRMultiLineString *)geom;					
			for(int l = 0; l < mls->getNumGeometries(); l++)
			{						
				OGRLineString *ls = (OGRLineString *)mls->getGeometryRef(l);
				readLineStringToLineCollection(ls, lines);						
			}
			lines->finishFeature();
		}
		else
			return false;

		return true;			
	};	
		
	readAnyCollection(lines.get(), layer, addFeature);

	lines->validate();
	return lines;
}

std::unique_ptr<PolygonCollection> OGRSourceOperator::getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) 
{
	auto polygons = make_unique<PolygonCollection>(rect);
	OGRLayer *layer = loadLayer(rect);

	auto addFeature = [&](OGRGeometry *geom, OGRFeature *feature, int featureCount) -> bool 
	{
		int type = wkbFlatten(geom->getGeometryType());			
		if(type == wkbPolygon)
		{
			OGRPolygon *polygon = (OGRPolygon *)geom;
			const OGRLinearRing *extRing = polygon->getExteriorRing();
			readRingToPolygonCollection(extRing, polygons);					

			for(int k = 0; k < polygon->getNumInteriorRings(); k++)
			{
				const OGRLinearRing *intRing = polygon->getInteriorRing(k);
				readRingToPolygonCollection(intRing, polygons);
			}
			polygons->finishPolygon();
			polygons->finishFeature();

		}				
		else if(type == wkbMultiPolygon)
		{
			OGRMultiPolygon *multiPolygon = (OGRMultiPolygon *)geom;

			for(int l = 0; l < multiPolygon->getNumGeometries(); l++)
			{
				OGRPolygon *polygon = (OGRPolygon *)multiPolygon->getGeometryRef(l);

				const OGRLinearRing *extRing = polygon->getExteriorRing();
				readRingToPolygonCollection(extRing, polygons);					

				for(int k = 0; k < polygon->getNumInteriorRings(); k++)
				{
					const OGRLinearRing *intRing = polygon->getInteriorRing(k);
					readRingToPolygonCollection(intRing, polygons);
				}
				polygons->finishPolygon();
			}
			polygons->finishFeature();
		} 
		else
			return false;
	
		return true;
	};

	readAnyCollection(polygons.get(), layer, addFeature);		
	
	polygons->validate();
	return polygons;
}

void OGRSourceOperator::readLineStringToLineCollection(const OGRLineString *line, std::unique_ptr<LineCollection> &collection)
{
	for(int l = 0; l < line->getNumPoints(); l++)
	{
		OGRPoint p;
		line->getPoint(l, &p);
		collection->addCoordinate(p.getX(), p.getY());
	}
	collection->finishLine();
}

void OGRSourceOperator::readRingToPolygonCollection(const OGRLinearRing *ring, std::unique_ptr<PolygonCollection> &collection)
{
	for(int l = 0; l < ring->getNumPoints(); l++)
	{
		OGRPoint p;
		ring->getPoint(l, &p);
		collection->addCoordinate(p.getX(), p.getY());
	}
	collection->finishRing();
}

//create the AttributeArrays for the FeatureCollection based on Attribute Fields in OGRLayer
//create string array with names for writing the attributes easier
void OGRSourceOperator::createAttributeArrays(OGRFeatureDefn *attributeDefn, AttributeArrays &attributeArrays)
{
	int attributeCount = attributeDefn->GetFieldCount();
	attributeNames = new std::string[attributeCount];

	for(int i = 0; i < attributeCount; i++)
	{
		OGRFieldDefn *fieldDefn = attributeDefn->GetFieldDefn(i);
		std::string name(fieldDefn->GetNameRef());		
		if(name == ""){
			throw OperatorException("OGR Source: an attribute has no name.");
		}

		if(wantedAttributes.find(name) != wantedAttributes.end())
		{			
			AttributeType wantedType = wantedAttributes[name];
			attributeNames[i] = name;
			//create numeric or textual attribute with the name in FeatureCollection
			if(wantedType == AttributeType::TEXTUAL)
				attributeArrays.addTextualAttribute(name, Unit::unknown());
			else if(wantedType == AttributeType::NUMERIC)
				attributeArrays.addNumericAttribute(name, Unit::unknown());
		} else
			attributeNames[i] = "";
	}

}

// write attribute values for the given attribute defitinition using the attributeNames
// returns false if an error occured and ErrorHandling is set to skip so that 
// the last written feature has to be removed again
bool OGRSourceOperator::readAttributesIntoCollection(AttributeArrays &attributeArrays, OGRFeatureDefn *attributeDefn, OGRFeature *feature, int featureIndex)
{
	for(int i = 0; i < attributeDefn->GetFieldCount(); i++)
	{
		//if attribute was not wanted the place in the array has an empty string
		if(attributeNames[i] == "")
			continue;

		OGRFieldDefn *fieldDefn  = attributeDefn->GetFieldDefn(i);
		AttributeType wantedType = wantedAttributes[attributeNames[i]];
		OGRFieldType type  		 = fieldDefn->GetType();
	
		if(wantedType == AttributeType::TEXTUAL)
		{		
			//kann es zu einem Fehler kommen?
			attributeArrays.textual(attributeNames[i]).set(featureIndex, feature->GetFieldAsString(i));	
		}
		else if(wantedType == AttributeType::NUMERIC)
		{
			if(type == OFTInteger)
				attributeArrays.numeric(attributeNames[i]).set(featureIndex, feature->GetFieldAsInteger(i));
			else if(type == OFTInteger64)
				attributeArrays.numeric(attributeNames[i]).set(featureIndex, feature->GetFieldAsInteger64(i));
			else if(type == OFTReal)
				attributeArrays.numeric(attributeNames[i]).set(featureIndex, feature->GetFieldAsDouble(i));
			else 
			{
				try 
				{
					double parsed = std::stod(feature->GetFieldAsString(i));					
					attributeArrays.numeric(attributeNames[i]).set(featureIndex, parsed);
				} 
				catch (...)
				{
					switch(errorHandling)
					{
						case ErrorHandling::ABORT:
							throw OperatorException("OGR Source: Attribute \"" + attributeNames[i] + "\" requested as numeric can not be parsed to double.");
							break;
						case ErrorHandling::SKIP:
							//TODO: Remove the last feature inserted to FeatureCollection
							return false;
							break;
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

void OGRSourceOperator::close(){
	delete[] attributeNames;
	attributeNames = NULL;
	GDALClose(dataset);
	dataset = NULL;
}

bool OGRSourceOperator::hasSuffix(const std::string &str, const std::string &suffix)
{
	return  str.size() >= suffix.size() && 
			str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}