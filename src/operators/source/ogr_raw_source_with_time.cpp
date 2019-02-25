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
 * - force_ogr_time_filter: bool. force external time filter via ogr layer, even though data types don't match. Might not work
 * 							(result: empty collection), but has better performance for wfs requests [optional, false if not provided]
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
class OGRRawSourceWithTimeOperator : public GenericOperator {
    public:
        OGRRawSourceWithTimeOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);

        ~OGRRawSourceWithTimeOperator() override = default;

        std::unique_ptr<PointCollection>
        getPointCollection(const QueryRectangle &rect, const QueryTools &tools) override;

        std::unique_ptr<LineCollection> getLineCollection(const QueryRectangle &rect, const QueryTools &tools) override;

        std::unique_ptr<PolygonCollection>
        getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) override;

    protected:
        void writeSemanticParameters(std::ostringstream &stream) override;

        void getProvenance(ProvenanceCollection &pc) override;

    private:
        auto extendFilenameWithTime(const QueryRectangle &rect) -> void;

        std::unique_ptr<OGRSourceUtil> ogrUtil;
        std::string filename;
};

OGRRawSourceWithTimeOperator::OGRRawSourceWithTimeOperator(int sourcecounts[], GenericOperator *sources[],
                                                           Json::Value &params) : GenericOperator(sourcecounts,
                                                                                                  sources) {
    assumeSources(0);

    std::string local_id = "data.ogr_raw_source_with_time";
    filename = params.get("filename", "").asString();
//    local_id.append(filename); //todo: does this need the layer_name added?

    ogrUtil = make_unique<OGRSourceUtil>(params, std::move(local_id));
}

REGISTER_OPERATOR(OGRRawSourceWithTimeOperator, "ogr_raw_source_with_time");

void OGRRawSourceWithTimeOperator::writeSemanticParameters(std::ostringstream &stream) {
    Json::Value &params = ogrUtil->getParameters();
    Json::FastWriter writer;
    stream << writer.write(params);
}

void OGRRawSourceWithTimeOperator::getProvenance(ProvenanceCollection &pc) {
    ogrUtil->getProvenance(pc);
}

std::unique_ptr<PointCollection>
OGRRawSourceWithTimeOperator::getPointCollection(const QueryRectangle &rect, const QueryTools &tools) {
    extendFilenameWithTime(rect);

    return ogrUtil->getPointCollection(rect, tools);
}

std::unique_ptr<LineCollection>
OGRRawSourceWithTimeOperator::getLineCollection(const QueryRectangle &rect, const QueryTools &tools) {
    extendFilenameWithTime(rect);

    return ogrUtil->getLineCollection(rect, tools);
}

std::unique_ptr<PolygonCollection>
OGRRawSourceWithTimeOperator::getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) {
    extendFilenameWithTime(rect);

    return ogrUtil->getPolygonCollection(rect, tools);
}

template< size_t N >
constexpr size_t str_length( char const (&)[N] )
{
    return N-1;
}

auto OGRRawSourceWithTimeOperator::extendFilenameWithTime(const QueryRectangle &rect) -> void {
    bool first_param = filename.find('?') == std::string::npos;

    std::stringstream filename_with_params;

    filename_with_params.setf(std::ios::fixed, std::ios::floatfield);
    filename_with_params.width(str_length("253402300799"));
    filename_with_params.precision(4);

    filename_with_params << filename;
    filename_with_params << (first_param ? "?" : "&") << "startTime=" << rect.t1;
    filename_with_params << "&endTime=" << rect.t2;
    filename_with_params << "&xMin=" << rect.x1;
    filename_with_params << "&xMax=" << rect.x2;
    filename_with_params << "&yMin=" << rect.y1;
    filename_with_params << "&yMax=" << rect.y2;

    ogrUtil->getParameters()["filename"] = filename_with_params.str();
}
