#include "operators/operator.h"
#include "datatypes/polygoncollection.h"
#include "datatypes/linecollection.h"
#include "datatypes/pointcollection.h"
#include "datatypes/simplefeaturecollection.h"
#include "util/ogr_source_util.h"
#include <json/json.h>
#include <Poco/URI.h>

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
class OGRRawSourceOperator : public GenericOperator {
    public:
        OGRRawSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);

        ~OGRRawSourceOperator() override = default;

        auto getPointCollection(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<PointCollection> override;

        auto getLineCollection(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<LineCollection> override;

        auto getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<PolygonCollection> override;

    protected:
        void writeSemanticParameters(std::ostringstream &stream) override;

        void getProvenance(ProvenanceCollection &pc) override;

    private:
        static auto is_data_url(const std::string &filename) -> bool;

        /// Create a GDAL Memfile and return its filename `/vsimem/<RANDOMCHARS>`
        ///
        /// The parts that require de-allocation are stored at `vsi_mem_file` and `vsi_mem_file_content`
        auto create_memfile(const std::string &filename) -> std::string;

        /// Swaps the `filename` of `params` with `vsi_mem_file_name`, calls `query_function` and swaps the old value back.
        /// Returns the result of `query_function`.
        template<typename T>
        auto swap_call_and_return(std::function<T()> query_function) -> T;

        std::unique_ptr<OGRSourceUtil> ogrUtil;

        std::unique_ptr<VSILFILE, std::function<void(VSILFILE *)>> vsi_mem_file; // must be de-allocated if it is not null
        std::unique_ptr<std::string> vsi_mem_file_content;
        std::unique_ptr<std::string> vsi_mem_file_name;
};

OGRRawSourceOperator::OGRRawSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params)
        : GenericOperator(sourcecounts, sources) {
    assumeSources(0);

    const auto filename = params.get("filename", "").asString();
    std::string local_id;

    if (is_data_url(filename)) {
        vsi_mem_file_name = std::make_unique<std::string>(create_memfile(filename));
        local_id = ""; // prevent security checks
    } else {
        //todo: does this need the layer_name added?
        local_id = "data.ogr_raw_source." + filename;
    }

    ogrUtil = std::make_unique<OGRSourceUtil>(params, std::move(local_id));
}

auto OGRRawSourceOperator::create_memfile(const std::string &filename) -> std::string {
    const auto comma_position = filename.find(',');

    if (comma_position == std::string::npos) {
        throw OperatorException("OGRRawSource: Invalid Data URI since it misses a comma");
    }
    if (comma_position >= filename.length()) {
        throw OperatorException("OGRRawSource: Invalid Data URI since the data part is empty");
    }

    const auto data_encoded = filename.substr(comma_position + 1);
    vsi_mem_file_content = std::make_unique<std::string>();

    Poco::URI::decode(data_encoded, *vsi_mem_file_content, false);

    auto mem_file_name = concat("/vsimem/", UserDB::createRandomToken(32));

    auto *vsi_file = VSIFileFromMemBuffer(
            mem_file_name.c_str(),
            const_cast<GByte *>(reinterpret_cast<const GByte *>(vsi_mem_file_content->c_str())),
            vsi_mem_file_content->length(),
            false
    );

    vsi_mem_file = std::unique_ptr<VSILFILE, std::function<void(VSILFILE *)>>(
            vsi_file,
            [=](VSILFILE *vsi_file) { VSIUnlink(mem_file_name.c_str()); }
    );

    return mem_file_name;
}

REGISTER_OPERATOR(OGRRawSourceOperator, "ogr_raw_source"); // NOLINT(cert-err58-cpp)

void OGRRawSourceOperator::writeSemanticParameters(std::ostringstream &stream) {
    Json::Value &params = ogrUtil->getParameters();
    Json::FastWriter writer;
    stream << writer.write(params);
}

void OGRRawSourceOperator::getProvenance(ProvenanceCollection &pc) {
    ogrUtil->getProvenance(pc);
}

auto OGRRawSourceOperator::getPointCollection(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<PointCollection> {
    return swap_call_and_return<std::unique_ptr<PointCollection> >([&]() {
        return ogrUtil->getPointCollection(rect, tools);
    });
}

auto OGRRawSourceOperator::getLineCollection(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<LineCollection> {
    return swap_call_and_return<std::unique_ptr<LineCollection> >([&]() {
        return ogrUtil->getLineCollection(rect, tools);
    });
}

auto OGRRawSourceOperator::getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<PolygonCollection> {
    return swap_call_and_return<std::unique_ptr<PolygonCollection> >([&]() {
        return ogrUtil->getPolygonCollection(rect, tools);
    });
}

auto OGRRawSourceOperator::is_data_url(const std::string &filename) -> bool {
    return filename.find("data:") == 0;
}

template<typename T>
auto OGRRawSourceOperator::swap_call_and_return(std::function<T()> query_function) -> T {
    if (vsi_mem_file_name) {
        auto filename = ogrUtil->getParameters()["filename"].asString();
        ogrUtil->getParameters()["filename"] = *vsi_mem_file_name;

        T result = query_function();

        ogrUtil->getParameters()["filename"] = filename;

        return result;
    }

    return query_function();
}
