#include <gdal/ogrsf_frmts.h>
#include "operators/operator.h"
#include "datatypes/polygoncollection.h"
#include "datatypes/linecollection.h"
#include "datatypes/pointcollection.h"
#include "datatypes/simplefeaturecollection.h"

#include "util/gdal_timesnap.h"
#include "util/exceptions.h"
#include "util/make_unique.h"
#include "util/configuration.h"
#include <json/json.h>
#include <sstream>
#include <iostream>


/*
	params:
		- filename
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


		OGRLayer* loadLayer(const QueryRectangle &rect);
		void readRingToPolygonCollection(const OGRLinearRing *ring, std::unique_ptr<PolygonCollection> &collection);
		void readLineStringToLineCollection(const OGRLineString *line, std::unique_ptr<LineCollection> &collection);
		void createAttributeArrays(OGRFeatureDefn *attributeDefn, AttributeArrays &collection);
		void readAttributesIntoCollection(AttributeArrays &attributeArrays, OGRFeatureDefn *attributeDefn, OGRFeature *feature, int featureIndex);

		void close();
		
};

OGRSourceOperator::OGRSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params) : GenericOperator(sourcecounts, sources) {
	assumeSources(0);	
	filename = params.get("filename", "").asString();	
}


OGRSourceOperator::~OGRSourceOperator() {
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

void OGRSourceOperator::getProvenance(ProvenanceCollection &pc) {
	std::string local_identifier 	= "data.ogr_source." + filename;
	/*Json::Value provenanceinfo 		= datasetJson["provenance"];

	if (provenanceinfo.isObject()) {
		pc.add(Provenance(	provenanceinfo.get("citation", "").asString(),
							provenanceinfo.get("license", "").asString(),
							provenanceinfo.get("uri", "").asString(),
							local_identifier));
	} else {
		pc.add(Provenance("", "", "", local_identifier));
	}*/		
}

OGRLayer* OGRSourceOperator::loadLayer(const QueryRectangle &rect){
		
	GDALAllRegister();
	
	dataset = (GDALDataset*)GDALOpenEx(filename.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);

	if(dataset == NULL){
		close();
		throw OperatorException("Can not load dataset");
	}

	if(dataset->GetLayerCount() < 1){
		close();
		throw OperatorException("No layers in OGR Dataset");
	}

	OGRLayer *layer;
	layer = dataset->GetLayer(0);

	layer->SetSpatialFilterRect(rect.x1, rect.y1, rect.x2, rect.y2);

	layer->ResetReading();

	return layer;
}

std::unique_ptr<PointCollection> OGRSourceOperator::getPointCollection(const QueryRectangle &rect, const QueryTools &tools) {
	auto points = make_unique<PointCollection>(rect);
	OGRLayer *layer = loadLayer(rect);
	OGRFeatureDefn *attributeDefn = layer->GetLayerDefn();
	createAttributeArrays(attributeDefn, points->feature_attributes);
	OGRFeature* feature;

	int featureCount = 0;

	while( (feature = layer->GetNextFeature()) != NULL )
	{
		int fields = feature->GetGeomFieldCount();

		for(int i = 0; i < fields; i++){

			OGRGeometry *geom = feature->GetGeomFieldRef(i);
			if(geom != NULL){
				
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
				{
					OGRFeature::DestroyFeature(feature);
					close();
					throw OperatorException("OGR Source: Dataset is not a Point Collection");				
				}
			}
		}

		readAttributesIntoCollection(points->feature_attributes, attributeDefn, feature, featureCount);		
		featureCount++;
		OGRFeature::DestroyFeature(feature);

	}
	close();
	points->validate();
	return points;
	//return points->filterBySpatioTemporalReferenceIntersection(rect);	//could be double filtering because of layer->SetSpatialFilterRect
}

std::unique_ptr<LineCollection> OGRSourceOperator::getLineCollection(const QueryRectangle &rect, const QueryTools &tools) {
	auto lines = make_unique<LineCollection>(rect);
	OGRLayer *layer = loadLayer(rect);
	OGRFeatureDefn *attributeDefn = layer->GetLayerDefn();
	createAttributeArrays(attributeDefn, lines->feature_attributes);
	OGRFeature* feature;
	int featureCount = 0;

	while( (feature = layer->GetNextFeature()) != NULL )
	{
		int fields = feature->GetGeomFieldCount();
		
		for(int i = 0; i < fields; i++)
		{

			OGRGeometry *geom = feature->GetGeomFieldRef(i);
			if(geom != NULL)
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
				{
					OGRFeature::DestroyFeature(feature);
					close();
					throw OperatorException("OGR Source: Dataset is not a Line Collection");
				}
			}
		}
		readAttributesIntoCollection(lines->feature_attributes, attributeDefn, feature, featureCount);		
		featureCount++;
		OGRFeature::DestroyFeature(feature);
	}
	close();
	lines->validate();
	return lines;
}

std::unique_ptr<PolygonCollection> OGRSourceOperator::getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) 
{
	auto polygons = make_unique<PolygonCollection>(rect);
	OGRLayer *layer = loadLayer(rect);
	OGRFeatureDefn *attributeDefn = layer->GetLayerDefn();
	createAttributeArrays(attributeDefn, polygons->feature_attributes);
	OGRFeature* feature;
	int featureCount = 0;

	while( (feature = layer->GetNextFeature()) != NULL )
	{
		int fields = feature->GetGeomFieldCount();		
		for(int i = 0; i < fields; i++)
		{
			OGRGeometry *geom = feature->GetGeomFieldRef(i);
			if(geom != NULL)
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
				{
					OGRFeature::DestroyFeature(feature);
					close();
					throw OperatorException("OGR Source: Dataset is not a Polygon Collection");
				}
			}
		}
		readAttributesIntoCollection(polygons->feature_attributes, attributeDefn, feature, featureCount);		
		featureCount++;
		OGRFeature::DestroyFeature(feature);
	}
	close();
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

void OGRSourceOperator::createAttributeArrays(OGRFeatureDefn *attributeDefn, AttributeArrays &attributeArrays)
{
	int attributeCount = attributeDefn->GetFieldCount();

	attributeNames = new std::string[attributeCount];

	for(int i = 0; i < attributeCount; i++)
	{
		OGRFieldDefn *fieldDefn = attributeDefn->GetFieldDefn(i);
		std::string name(fieldDefn->GetNameRef());		
		if(name == ""){
			close();
			throw OperatorException("OGR Source: an attribute has no name.");
		}

		attributeNames[i] = name;

		std::cout << "Attribute " << i << ": " << name;
		OGRFieldType type  = fieldDefn->GetType();
		std::cout << ". Type: " << type << std::endl;

		if(type == OFTInteger || type == OFTInteger64 || type == OFTReal)
			attributeArrays.addNumericAttribute(name, Unit::unknown());
		else
			attributeArrays.addTextualAttribute(name, Unit::unknown());
		
	}

}

void OGRSourceOperator::readAttributesIntoCollection(AttributeArrays &attributeArrays, OGRFeatureDefn *attributeDefn, OGRFeature *feature, int featureIndex)
{
	for(int i = 0; i < attributeDefn->GetFieldCount(); i++){
		OGRFieldDefn *fieldDefn = attributeDefn->GetFieldDefn(i);

		OGRFieldType type  = fieldDefn->GetType();
	
		if(type == OFTInteger)
			attributeArrays.numeric(attributeNames[i]).set(featureIndex, feature->GetFieldAsInteger(i));
		else if(type == OFTInteger64)
			attributeArrays.numeric(attributeNames[i]).set(featureIndex, feature->GetFieldAsInteger64(i));
		else if(type == OFTReal)
			attributeArrays.numeric(attributeNames[i]).set(featureIndex, feature->GetFieldAsDouble(i));
		else
			attributeArrays.textual(attributeNames[i]).set(featureIndex, feature->GetFieldAsString(i));	
	}

}

void OGRSourceOperator::close(){
	delete[] attributeNames;
	GDALClose(dataset);
}