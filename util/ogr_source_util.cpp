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
#include <unordered_map>
#include <unordered_set>
#include <json/json.h>


OGRSourceUtil::OGRSourceUtil(Json::Value &params)
{	
	errorHandling = ErrorHandlingConverter.from_json(params, "on_error");	

	columns = params.get("columns", Json::Value(Json::ValueType::objectValue));
	auto textual = columns.get("textual", Json::Value(Json::ValueType::arrayValue));
    for (auto &name : textual)
    	wantedAttributes[name.asString()] = AttributeType::TEXTUAL;
    
    auto numeric = columns.get("numeric", Json::Value(Json::ValueType::arrayValue));
    for (auto &name : numeric)
    	wantedAttributes[name.asString()] = AttributeType::NUMERIC;

    timeSpecification = TimeSpecificationConverter.from_json(params, "time");

    timeDuration = 0.0;
	if (timeSpecification == TimeSpecification::START) {
		if(!params.isMember("duration"))
			throw ArgumentException("CSVSource: TimeSpecification::Start chosen, but no duration given.");

		auto duration = params.get("duration", Json::Value());
		if(duration.isString() && duration.asString() == "inf")
			timeDuration = -1.0;
		else if (duration.isNumeric())
			timeDuration = duration.asDouble();
		else
			throw ArgumentException("CSVSource: invalid duration given.");
	}

    if(timeSpecification != TimeSpecification::NONE){
		time1Name = columns.get("time1", "time1").asString();

		const Json::Value& time1Format = params.get("time1_format", Json::Value::null);
		time1Parser = TimeParser::createFromJson(time1Format);
	}

	if(timeSpecification == TimeSpecification::START_DURATION || timeSpecification == TimeSpecification::START_END){
		//TODO: check that time2 can be used as interval (e.g. format::seconds)
		time2Name = columns.get("time2", "time2").asString();

		const Json::Value& time2Format = params.get("time2_format", Json::Value::null);
		time2Parser = TimeParser::createFromJson(time2Format);
	}
}

OGRSourceUtil::~OGRSourceUtil(){
	delete[] attributeNames;
	attributeNames = NULL;	
}


void OGRSourceUtil::readAnyCollection(const QueryRectangle &rect, SimpleFeatureCollection *collection, OGRLayer *layer, std::function<bool(OGRGeometry *, OGRFeature *, int)> addFeature)
{
	OGRFeatureDefn *attributeDefn = layer->GetLayerDefn();
	createAttributeArrays(attributeDefn, collection->feature_attributes);
	initTimeReading(attributeDefn);
	OGRFeature* feature;

	int featureCount = 0;

	while( (feature = layer->GetNextFeature()) != NULL )
	{
		//each feature has potentially multiple geometries
		if(feature->GetGeomFieldCount() > 1){			
			throw OperatorException("OGR Source: at least one OGRFeature has more than one Geometry. For now we don't support this.");
		}
		
		OGRGeometry *geom = feature->GetGeomFieldRef(0);
		bool success = false;

		if(geom != NULL)
		{			
			if(!readTimeIntoCollection(rect, feature, collection->time)){		
				//error returned, either ErrorHandling::SKIP or the time stamps of feature were filtered -> so continue to next feature	
				continue;
			} else
				success = addFeature(geom, feature, featureCount);
		}			

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
					if(geom == NULL)
						throw OperatorException("OGR Source: Invalid dataset, at least one OGRGeometry was NULL.");
					else
						throw OperatorException("OGR Source: Dataset contains not expected FeatureType (Points,Lines,Polygons)");
					break;
				case ErrorHandling::SKIP:
					//removing the last written feature is handled in addFeature or after the error occurred because here 
					//we can not know if writing has actually started or if a type missmatch happened.
					break;
				case ErrorHandling::KEEP:
					//TODO: ???? Insert 0-Feature?
					break;
			}
		}			
		OGRFeature::DestroyFeature(feature);	
	}

	// if it is wanted to insert default timestamps for TimeSpec.::NONE uncomment this.
	// for now we decided not to add them.
	/*if(timeSpecification == TimeSpecification::NONE)
		collection->addDefaultTimestamps();*/

}


std::unique_ptr<PointCollection> OGRSourceUtil::getPointCollection(const QueryRectangle &rect, const QueryTools &tools, OGRLayer *layer)
{
	auto points = make_unique<PointCollection>(rect);
	
	//OGRFeatureDefn *attributeDefn = layer->GetLayerDefn();

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

	readAnyCollection(rect, points.get(), layer, addFeature);
	points->validate();
	return points;
}

std::unique_ptr<LineCollection> OGRSourceUtil::getLineCollection(const QueryRectangle &rect, const QueryTools &tools, OGRLayer *layer)
{
	auto lines = make_unique<LineCollection>(rect);	

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
		
	readAnyCollection(rect, lines.get(), layer, addFeature);
	lines->validate();
	return lines;
}

std::unique_ptr<PolygonCollection> OGRSourceUtil::getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools, OGRLayer *layer) 
{
	auto polygons = make_unique<PolygonCollection>(rect);	

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

	readAnyCollection(rect, polygons.get(), layer, addFeature);		
	polygons->validate();
	return polygons;
}

void OGRSourceUtil::readLineStringToLineCollection(const OGRLineString *line, std::unique_ptr<LineCollection> &collection)
{
	for(int l = 0; l < line->getNumPoints(); l++)
	{
		OGRPoint p;
		line->getPoint(l, &p);
		collection->addCoordinate(p.getX(), p.getY());
	}
	collection->finishLine();
}

void OGRSourceUtil::readRingToPolygonCollection(const OGRLinearRing *ring, std::unique_ptr<PolygonCollection> &collection)
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
void OGRSourceUtil::createAttributeArrays(OGRFeatureDefn *attributeDefn, AttributeArrays &attributeArrays)
{
	int attributeCount = attributeDefn->GetFieldCount();
	attributeNames = new std::string[attributeCount];
	std::unordered_set<std::string> existingAttributes;

	for(int i = 0; i < attributeCount; i++)
	{
		OGRFieldDefn *fieldDefn = attributeDefn->GetFieldDefn(i);
		std::string name(fieldDefn->GetNameRef());		
		if(name == ""){
			throw OperatorException("OGR Source: an attribute has no name.");
		}
		existingAttributes.insert(name);
		if(wantedAttributes.find(name) != wantedAttributes.end())
		{			
			AttributeType wantedType = wantedAttributes[name];
			attributeNames[i] = name;
			//create numeric or textual attribute with the name in FeatureCollection
			if(wantedType == AttributeType::TEXTUAL)
				attributeArrays.addTextualAttribute(name, Unit::unknown()); //TODO: units
			else if(wantedType == AttributeType::NUMERIC)
				attributeArrays.addNumericAttribute(name, Unit::unknown()); //TODO: units
		} else
			attributeNames[i] = "";
	}

	// check if all requested attributes exist in FeatureDefn
	for(auto wanted : wantedAttributes)
	{
		if(existingAttributes.find(wanted.first) == existingAttributes.end())				
			throw OperatorException("OGR Source: The requested attribute " + wanted.first + " does not exist in source file.");		
	}
}

// write attribute values for the given attribute defitinition using the attributeNames
// returns false if an error occured and ErrorHandling is set to skip so that 
// the last written feature has to be removed again in the calling function
bool OGRSourceUtil::readAttributesIntoCollection(AttributeArrays &attributeArrays, OGRFeatureDefn *attributeDefn, OGRFeature *feature, int featureIndex)
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

void OGRSourceUtil::initTimeReading(OGRFeatureDefn *attributeDefn)
{
	if(timeSpecification == TimeSpecification::NONE)
		return;

	time1Index = -1;
	time2Index = -1;

	for(int i = 0; i < attributeDefn->GetFieldCount(); i++)
	{
		OGRFieldDefn *fieldDefn = attributeDefn->GetFieldDefn(i);
		std::string name(fieldDefn->GetNameRef());
		if(name == time1Name)
			time1Index = i;
		else if(name == time2Name)
			time2Index = i;
	}

	if(time1Index == -1)
		throw OperatorException("OGR Source: time1 attribute not found.");
	
	if(timeSpecification == TimeSpecification::START_DURATION || timeSpecification == TimeSpecification::START_END)
	{
		if(time2Index == -1)
			throw OperatorException("OGR Source: time1 attribute not found.");
	}
}

bool OGRSourceUtil::readTimeIntoCollection(const QueryRectangle &rect, OGRFeature *feature, std::vector<TimeInterval> &time)
{
	if(timeSpecification == TimeSpecification::NONE)
		return true;	

	double t1, t2;
	bool error = false;
	if (timeSpecification == TimeSpecification::START) {
		try {
			t1 = time1Parser->parse(feature->GetFieldAsString(time1Index));
			if(timeDuration >= 0.0)
				t2 = t1 + timeDuration;
			else
				t2 = rect.end_of_time();
		} catch (const TimeParseException& e){
			t1 = rect.beginning_of_time();
			t2 = rect.end_of_time();
			error = true;
		}
	}
	else if (timeSpecification == TimeSpecification::START_END) {
		try {
			t1 = time1Parser->parse(feature->GetFieldAsString(time1Index));
		} catch (const TimeParseException& e){
			t1 = rect.beginning_of_time();
			error = true;
		}
		try {
			t2 = time2Parser->parse(feature->GetFieldAsString(time2Index));
		} catch (const TimeParseException& e){
			t2 = rect.end_of_time();
			error = true;
		}
	}
	else if (timeSpecification == TimeSpecification::START_DURATION) {
		try {
			t1 = time1Parser->parse(feature->GetFieldAsString(time1Index));
			t2 = t1 + time2Parser->parse(feature->GetFieldAsString(time2Index));
		} catch (const TimeParseException& e){
			t1 = rect.beginning_of_time();
			t2 = rect.end_of_time();
			error = true;
		}
	}

	if(error) {
		switch(errorHandling) {
			case ErrorHandling::ABORT:
				throw OperatorException("OGR Source: Could not parse time.");
			case ErrorHandling::SKIP:
				return false;
			case ErrorHandling::KEEP:
				break;
		}
	}
	
	//check if features time stamps have overlap with query rectangle times
	if(t1 < rect.t1 && t2 < rect.t1 || t1 > rect.t1 && t2 > rect.t2)
		return false;

	time.push_back(TimeInterval(t1, t2));	
	return true;
}

Json::Value OGRSourceUtil::getColumnsJson()
{
	return columns;
}