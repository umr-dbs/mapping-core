#include <gdal/ogrsf_frmts.h>
#include "operators/operator.h"
#include "datatypes/polygoncollection.h"
#include "datatypes/linecollection.h"
#include "datatypes/pointcollection.h"
#include "datatypes/simplefeaturecollection.h"
#include "util/ogr_source_util.h"
#include <json/json.h>
#include <iostream>

/** Operator for opening OGR/GDAL supported vector data files as FeatureCollections.
 *  Main implementation of feature reading can be found in util/ogr_source_util.h/cpp.
 *  Difference to OGRSourceOperator: All data has to be provided in query, nothing is defined in datasets on disk.
 *
 * Query Parameters:
 * - filename: path to the input file
 * - layer_name: name of the layer to load
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
 * - columns: a json object mapping the columns to data, time, space. Columns that are not listed are skipped when parsing.
 *   - x: the name of the column containing the x coordinate (or the wkt string) [if CSV file]
 *   - y: the name of the column containing the y coordinate [if CSV file with y column]
 *   - time1: the name of the first time column [if time != "none"]
 *   - time2: the name of the second time column [if time == "start+end" || "start+duration"]
 *   - numeric: an array of column names containing numeric values
 *   - textual: an array of column names containing alpha-numeric values
 * - default: wkt defintion of the default point/line/polygon as a string [optional]
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
class OGRRawSourceOperator : public GenericOperator {
public:
	OGRRawSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);
	virtual ~OGRRawSourceOperator() override = default;

	std::unique_ptr<PointCollection> getPointCollection(const QueryRectangle &rect, const QueryTools &tools) override;
	std::unique_ptr<LineCollection> getLineCollection(const QueryRectangle &rect, const QueryTools &tools) override;
	std::unique_ptr<PolygonCollection> getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) override;

protected:
	void writeSemanticParameters(std::ostringstream& stream) override;
	void getProvenance(ProvenanceCollection &pc) override;

private:
	std::unique_ptr<OGRSourceUtil> ogrUtil;
};

OGRRawSourceOperator::OGRRawSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params) : GenericOperator(sourcecounts, sources) {
	assumeSources(0);

	Provenance provenance;
	std::string filename = params.get("filename", "").asString();
	Json::Value provenanceInfo = params["provenance"];
	if (provenanceInfo.isObject()) {
		provenance = Provenance(provenanceInfo.get("citation", "").asString(),
								provenanceInfo.get("license", "").asString(),
								provenanceInfo.get("uri", "").asString(),
								"data.ogr_raw_source." + filename); //todo: does this need the layer_name added?
	}
	ogrUtil = make_unique<OGRSourceUtil>(params, provenance);
}

REGISTER_OPERATOR(OGRRawSourceOperator, "ogr_raw_source");

void OGRRawSourceOperator::writeSemanticParameters(std::ostringstream& stream)
{
	ogrUtil->writeSemanticParametersRaw(stream);
}

void OGRRawSourceOperator::getProvenance(ProvenanceCollection &pc)
{	
	ogrUtil->getProvenance(pc);
}

std::unique_ptr<PointCollection> OGRRawSourceOperator::getPointCollection(const QueryRectangle &rect, const QueryTools &tools)
{
	return ogrUtil->getPointCollection(rect, tools);
}

std::unique_ptr<LineCollection> OGRRawSourceOperator::getLineCollection(const QueryRectangle &rect, const QueryTools &tools)
{
	return ogrUtil->getLineCollection(rect, tools);
}

std::unique_ptr<PolygonCollection> OGRRawSourceOperator::getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools)
{	
	return ogrUtil->getPolygonCollection(rect, tools);
}
