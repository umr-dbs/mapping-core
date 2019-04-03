
#include <iostream>
#include "operators/operator.h"
#include "util/ogr_source_util.h"
#include "util/ogr_source_datasets.h"

/**
 * Operator for loading vector datasets by name. Additionally the name of the requested layer has to be provided and
 * arrays of requested attribute names for textual and numeric attributes. The available layers and attributes of
 * each layer are provided by the source listing of the user service.
 * Difference to OGRRawSourceOperator is that not all query parameters need to be provided when calling the operator.
 * These information, like time attributes, geometry attributes for csv files, etc. are saved on disk in a dataset
 * description json. This json file is loaded by this operator and used for reading the feature collection.
 *
 *  params:
 *      - name: name of json OGRSource data json to be opened.
 *      - layer_name: name of requested layer.
 *      - textual: array of names of the requested textual attributes.
 *      - numeric: array of names of the requested numeric attributes.
 */

class OGRSourceOperator : public GenericOperator {
public:
    OGRSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);
    ~OGRSourceOperator() override = default ;
    std::unique_ptr<PointCollection> getPointCollection(const QueryRectangle &rect, const QueryTools &tools) override;
    std::unique_ptr<LineCollection> getLineCollection(const QueryRectangle &rect, const QueryTools &tools) override;
    std::unique_ptr<PolygonCollection> getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) override;

protected:
    void writeSemanticParameters(std::ostringstream& stream) override;
    void getProvenance(ProvenanceCollection &pc) override;
private:
    /**
     * Constructs the json parameters needed for opening the feature collection from dataset and layer definition.
     * The parameters defined for the layer are prioritized.
     * @param params: Parameters of the query for the OGR Source Operator.
     * @return Json object containing the query parameters.
     */
    Json::Value constructParameters(Json::Value &params);

    std::unique_ptr<OGRSourceUtil> ogrUtil;
    std::string dataset_name;
};

OGRSourceOperator::OGRSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params)
        : GenericOperator(sourcecounts, sources)
{
    assumeSources(0);
    dataset_name = params["name"].asString();
    std::string local_id = "data.ogr_source.";
    local_id.append(dataset_name); //todo: does this need the layer_name added?
    ogrUtil = std::make_unique<OGRSourceUtil>(constructParameters(params), std::move(local_id));
}

REGISTER_OPERATOR(OGRSourceOperator, "ogr_source");

std::unique_ptr<PointCollection>
OGRSourceOperator::getPointCollection(const QueryRectangle &rect, const QueryTools &tools) {
    return ogrUtil->getPointCollection(rect, tools);
}

std::unique_ptr<LineCollection>
OGRSourceOperator::getLineCollection(const QueryRectangle &rect, const QueryTools &tools) {
    return ogrUtil->getLineCollection(rect, tools);
}

std::unique_ptr<PolygonCollection>
OGRSourceOperator::getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) {
    return ogrUtil->getPolygonCollection(rect, tools);
}

void OGRSourceOperator::writeSemanticParameters(std::ostringstream &stream) {
    Json::Value root;

    Json::Value& params = ogrUtil->getParameters();

    root["name"]        = dataset_name;
    root["layer_name"]  = params["layer_name"];
    root["textual"]     = params["columns"]["textual"];
    root["numeric"]     = params["columns"]["numeric"];

    stream << root;
}

void OGRSourceOperator::getProvenance(ProvenanceCollection &pc) {
    ogrUtil->getProvenance(pc);
}


Json::Value OGRSourceOperator::constructParameters(Json::Value &params){
    Json::Value dataset_json            = OGRSourceDatasets::getDatasetDescription(dataset_name);
    const std::string layer_name        = params["layer_name"].asString();
    Json::Value layer_json              = dataset_json["layers"][layer_name];

    // create new json object from the queries parameters, the dataset definition, and the layer definition:
    Json::Value constructed_params(Json::ValueType::objectValue);
    constructed_params["filename"]    = OGRSourceDatasets::getJsonParameter(layer_json, dataset_json, "filename");
    constructed_params["layer_name"]  = params["layer_name"];

    constructed_params["time"]        = OGRSourceDatasets::getJsonParameterDefault(layer_json, dataset_json, "time", "none");

    if(OGRSourceDatasets::hasJsonParameter(layer_json, dataset_json, "duration"))
        constructed_params["duration"] = OGRSourceDatasets::getJsonParameter(layer_json, dataset_json, "duration");

    if(OGRSourceDatasets::hasJsonParameter(layer_json, dataset_json, "time1_format"))
        constructed_params["time1_format"] = OGRSourceDatasets::getJsonParameter(layer_json, dataset_json, "time1_format");

    if(OGRSourceDatasets::hasJsonParameter(layer_json, dataset_json, "time2_format"))
        constructed_params["time2_format"] = OGRSourceDatasets::getJsonParameter(layer_json, dataset_json, "time2_format");

    if(OGRSourceDatasets::hasJsonParameter(layer_json, dataset_json, "default"))
        constructed_params["default"] = OGRSourceDatasets::getJsonParameter(layer_json, dataset_json, "default");

    if(OGRSourceDatasets::hasJsonParameter(layer_json, dataset_json, "force_ogr_time_filter"))
        constructed_params["force_ogr_time_filter"] = OGRSourceDatasets::getJsonParameter(layer_json, dataset_json, "force_ogr_time_filter");

    if(OGRSourceDatasets::hasJsonParameter(layer_json, dataset_json, "on_error"))
        constructed_params["on_error"] = OGRSourceDatasets::getJsonParameter(layer_json, dataset_json, "on_error");

    // columns
    Json::Value columns(Json::ValueType::objectValue);
    Json::Value layer_columns   = layer_json["columns"];
    Json::Value dataset_columns = dataset_json["columns"];

    if(OGRSourceDatasets::hasJsonParameter(layer_columns, dataset_columns, "x"))
        columns["x"] = OGRSourceDatasets::getJsonParameter(layer_columns, dataset_columns, "x");
    if(OGRSourceDatasets::hasJsonParameter(layer_columns, dataset_columns, "y"))
        columns["y"] = OGRSourceDatasets::getJsonParameter(layer_columns, dataset_columns, "y");

    if(OGRSourceDatasets::hasJsonParameter(layer_columns, dataset_columns, "time1"))
        columns["time1"] = OGRSourceDatasets::getJsonParameter(layer_columns, dataset_columns, "time1");
    if(OGRSourceDatasets::hasJsonParameter(layer_columns, dataset_columns, "time2"))
        columns["time2"] = OGRSourceDatasets::getJsonParameter(layer_columns, dataset_columns, "time2");

    columns["textual"]            = params["textual"];
    columns["numeric"]            = params["numeric"];
    constructed_params["columns"] = columns;

    // provenance information
    Json::Value dataset_provenance = dataset_json["provenance"];
    Json::Value layer_provenance   = layer_json["provenance"];
    Json::Value provenanceInfo;
    if(OGRSourceDatasets::hasJsonParameter(layer_provenance, dataset_provenance, "citation"))
        provenanceInfo["citation"] = OGRSourceDatasets::getJsonParameter(layer_provenance, dataset_provenance, "citation");

    if(OGRSourceDatasets::hasJsonParameter(layer_provenance, dataset_provenance, "license"))
        provenanceInfo["license"] = OGRSourceDatasets::getJsonParameter(layer_provenance, dataset_provenance, "license");

    if(OGRSourceDatasets::hasJsonParameter(layer_provenance, dataset_provenance, "uri"))
        provenanceInfo["uri"] = OGRSourceDatasets::getJsonParameter(layer_provenance, dataset_provenance, "uri");

    constructed_params["provenance"] = provenanceInfo;

    return constructed_params;
}
