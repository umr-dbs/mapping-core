
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
 * description json. This json file is loaded from this operator and used for reading the feature collection.
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
    std::unique_ptr<OGRSourceUtil> ogrUtil;
};

OGRSourceOperator::OGRSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params)
        : GenericOperator(sourcecounts, sources)
{
    assumeSources(0);

    std::string name                    = params["name"].asString();
    Json::Value loaded_params           = OGRSourceDatasets::getFileDescription(name);

    if(params.isMember("layer_name"))
        loaded_params["layer_name"]     = params["layer_name"];
    loaded_params["columns"]["textual"] = params["textual"];
    loaded_params["columns"]["numeric"] = params["numeric"];


    Provenance provenance;
    std::string filename = loaded_params.get("filename", "").asString();
    Json::Value provenanceInfo = loaded_params["provenance"];
    if (provenanceInfo.isObject()) {
        provenance = Provenance(provenanceInfo.get("citation", "").asString(),
                                provenanceInfo.get("license", "").asString(),
                                provenanceInfo.get("uri", "").asString(),
                                "data.ogr_raw_source." + filename);
    }

    ogrUtil = make_unique<OGRSourceUtil>(loaded_params, provenance);
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
    ogrUtil->writeSemanticParameters(stream);
}

void OGRSourceOperator::getProvenance(ProvenanceCollection &pc) {
    ogrUtil->getProvenance(pc);
}
