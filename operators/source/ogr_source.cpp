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


/*
	params:
		- sourcename
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
		std::string datasetPath;
		std::string sourcename;
		Json::Value datasetJson;

		GDALDataset *dataset;

		OGRLayer* loadLayer(const QueryRectangle &rect);
		
};

OGRSourceOperator::OGRSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params) : GenericOperator(sourcecounts, sources) {
	assumeSources(0);

	datasetPath = Configuration::get("ogrsource.datasetpath");
	sourcename 	= params.get("sourcename", "").asString();
	datasetJson = GDALTimesnap::getDatasetJson(sourcename, datasetPath);
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
	std::string local_identifier 	= "data.ogr_source." + sourcename;
	Json::Value provenanceinfo 		= datasetJson["provenance"];

	if (provenanceinfo.isObject()) {
		pc.add(Provenance(	provenanceinfo.get("citation", "").asString(),
							provenanceinfo.get("license", "").asString(),
							provenanceinfo.get("uri", "").asString(),
							local_identifier));
	} else {
		pc.add(Provenance("", "", "", local_identifier));
	}		
}

OGRLayer* OGRSourceOperator::loadLayer(const QueryRectangle &rect){

	std::string file = GDALTimesnap::getDatasetFilename(datasetJson, rect.t1);
	
	GDALAllRegister();
	
	dataset = (GDALDataset*)GDALOpenEx(file.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);

	if(dataset == NULL){
		throw OperatorException("Cant load dataset");
	}

	if(dataset->GetLayerCount() < 1){
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
	OGRFeature* feature;

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
					throw OperatorException("OGR Source: Dataset is not a Point Collection");				
			}
		}

		OGRFeature::DestroyFeature(feature);

	}

	GDALClose(dataset);
	//return points;
	return points->filterBySpatioTemporalReferenceIntersection(rect);	//could be double filtering because of layer->SetSpatialFilterRect
}

std::unique_ptr<LineCollection> OGRSourceOperator::getLineCollection(const QueryRectangle &rect, const QueryTools &tools) {
	auto lines = make_unique<LineCollection>(rect);
	OGRLayer *layer = loadLayer(rect);
	OGRFeature* feature;

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
					for(int k = 0; k < ls->getNumPoints(); k++)
					{
						OGRPoint p;
						ls->getPoint(k, &p);
						lines->addCoordinate(p.getX(), p.getY());
					}
					lines->finishLine();
					lines->finishFeature();
				}
				else if(type == wkbMultiLineString)
				{
					OGRMultiLineString *mls = (OGRMultiLineString *)geom;					
					for(int l = 0; l < mls->getNumGeometries(); l++)
					{						
						OGRLineString *ls = (OGRLineString *)mls->getGeometryRef(l);
						for(int k = 0; k < ls->getNumPoints(); k++){
							OGRPoint p;
							ls->getPoint(k, &p);
							lines->addCoordinate(p.getX(), p.getY());
						}
						lines->finishLine();
					}
					lines->finishFeature();
				}
				else 
					throw OperatorException("OGR Source: Dataset is not a Line Collection");
			}
		}
		OGRFeature::DestroyFeature(feature);
	}

	GDALClose(dataset);
	return lines;
}

std::unique_ptr<PolygonCollection> OGRSourceOperator::getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) {
	auto polygons = make_unique<PolygonCollection>(rect);
	OGRLayer *layer = loadLayer(rect);
	OGRFeature* feature;

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

				}				
				else if(type == wkbMultiPolygon)
				{
					
				} 
				else
					throw OperatorException("OGR Source: Dataset is not a Polygon Collection");
			}
		}
		OGRFeature::DestroyFeature(feature);
	}

	GDALClose(dataset);
	return polygons;
}