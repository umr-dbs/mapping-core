#include <gdal/ogrsf_frmts.h>
#include "operators/operator.h"
#include "datatypes/polygoncollection.h"
#include "datatypes/linecollection.h"
#include "datatypes/pointcollection.h"
#include "datatypes/simplefeaturecollection.h"
#include "util/ogr_source_util.h"
#include <json/json.h>

/* Operator for opening OGR/GDAL supported vector data files as FeatureCollections.
 * Main implementation of feature reading can be found in util/ogr_source_util.h/cpp.
 *
 * Query Parameters:
 * - filename: path to the input file
 * - time: the type of the time column(s)
 *   - "none": no time information is mapped
 *   - "start": only start information is mapped. duration has to specified in the duration attribute
 *   - "start+end": start and end information is mapped
 *   - "start+duration": start and duration information is mapped
 * -  duration: the duration of the time validity for all features in the file [if time == "duration"]
 * - time1_format: a json object mapping a column to the start time [if time != "none"]
 *   - format: define the format of the column
 *     - "custom": define a custom format in the attribute "custom_format"
 *     - "seconds": time column is numeric and contains seconds as UNIX timestamp
 *     - "dmyhm": %d-%B-%Y  %H:%M
 *     - "iso": time column contains string with ISO8601
 * - time2_format: a json object mapping a columns to the end time (cf. time1_format) [if time == "start+end" || "start+duration"]
 * - columns: a json object mapping the columns to data, time, space. Columns that are not listed are skipped when parsin.
 *   - x: the name of the column containing the x coordinate (or the wkt string) [if CSV file]
 *   - y: the name of the column containing the y coordinate [if CSV file with y column]
 *   - time1: the name of the first time column [if time != "none"]
 *   - time2: the name of the second time column [if time == "start+end" || "start+duration"]
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
		std::unique_ptr<OGRSourceUtil> ogrUtil;
		GDALDataset *dataset;	
		Provenance provenance;		
		std::string filename;
		Json::Value columns;

		OGRLayer* loadLayer(const QueryRectangle &rect);		
		bool hasSuffix(const std::string &str, const std::string &suffix);		
		void close();
};

OGRSourceOperator::OGRSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params) : GenericOperator(sourcecounts, sources) {
	assumeSources(0);
	filename = params.get("filename", "").asString();
	ogrUtil = make_unique<OGRSourceUtil>(params);
	columns = params.get("columns", Json::Value(Json::ValueType::objectValue));
	Json::Value provenanceInfo = params["provenance"];
	if (provenanceInfo.isObject()) {
		provenance = Provenance(provenanceInfo.get("citation", "").asString(),
				provenanceInfo.get("license", "").asString(),
				provenanceInfo.get("uri", "").asString(), "data.ogr_source." + filename);
	}  
}

REGISTER_OPERATOR(OGRSourceOperator, "ogr_source");

OGRSourceOperator::~OGRSourceOperator()
{
	close();
}

void OGRSourceOperator::close(){	
	GDALClose(dataset);
	dataset = NULL;
}


void OGRSourceOperator::writeSemanticParameters(std::ostringstream& stream)
{
	// what to write here? the code beneath is copied from CSVSource
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

// loads the OGR Layer for the filename. for .csv files open options are read from query parametrs
OGRLayer* OGRSourceOperator::loadLayer(const QueryRectangle &rect)
{		
	GDALAllRegister();

	bool isCsv = hasSuffix(filename, ".csv") || hasSuffix(filename, ".tsv");

	// if its a csv file we have to add open options to tell gdal what the geometry columns are.
	if(isCsv){		
		std::string column_x = columns.get("x", "x").asString();
		
		if(columns.isMember("y"))
		{	
			std::string column_y 	= columns.get("y", "y").asString();
			std::string optX 		= "X_POSSIBLE_NAMES=" + column_x;			
			std::string optY 		= "Y_POSSIBLE_NAMES=" + column_y;
			const char * const strs[] = { optX.c_str(), optY.c_str(), NULL};									
			dataset = (GDALDataset*)GDALOpenEx(filename.c_str(), GDAL_OF_VECTOR, NULL, strs, NULL);			
		} 
		else
		{
			std::string opt = "GEOM_POSSIBLE_NAMES=" + column_x;				
			const char * const strs[] = { opt.c_str(), NULL };
			dataset = (GDALDataset*)GDALOpenEx(filename.c_str(), GDAL_OF_VECTOR, NULL, strs, NULL);
		}		
	} 
	else
		dataset = (GDALDataset*)GDALOpenEx(filename.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);

	if(dataset == NULL){
		close();
		throw OperatorException("OGR Source: Can not load dataset");
	}

	if(dataset->GetLayerCount() < 1){
		close();
		throw OperatorException("OGR Source: No layers in OGR Dataset");
	}

	OGRLayer *layer;

	// for now only take layer 0 in consideration
	layer = dataset->GetLayer(0);

	if(layer == NULL){
		close();
		throw OperatorException("OGR Source: Layer could not be read from dataset.");
	}

	// filters all Features not intersecting with the query rectangle.
	layer->SetSpatialFilterRect(rect.x1, rect.y1, rect.x2, rect.y2);

	// a call suggested by OGR Tutorial as "just in case" (to start reading from first feature)
	layer->ResetReading();

	return layer;
}

bool OGRSourceOperator::hasSuffix(const std::string &str, const std::string &suffix)
{
	return  str.size() >= suffix.size() && 
			str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::unique_ptr<PointCollection> OGRSourceOperator::getPointCollection(const QueryRectangle &rect, const QueryTools &tools)
{
	OGRLayer *layer = loadLayer(rect);
	return ogrUtil->getPointCollection(rect, tools, layer);
}

std::unique_ptr<LineCollection> OGRSourceOperator::getLineCollection(const QueryRectangle &rect, const QueryTools &tools)
{
	OGRLayer *layer = loadLayer(rect);
	return ogrUtil->getLineCollection(rect, tools, layer);
}

std::unique_ptr<PolygonCollection> OGRSourceOperator::getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) 
{	
	OGRLayer *layer = loadLayer(rect);
	return ogrUtil->getPolygonCollection(rect, tools, layer);
}