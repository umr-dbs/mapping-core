#include "datatypes/pointcollection.h"
#include "datatypes/linecollection.h"
#include "datatypes/polygoncollection.h"

#include "operators/operator.h"

#include <json/json.h>
#include <util/enumconverter.h>
#include <algorithm>

enum class EngineType {
    EXACT, CONTAINS, STARTSWITH,
};

static const std::vector< std::pair<EngineType, std::string>> engine_type_map {
        std::make_pair(EngineType::EXACT, static_cast<std::string>("exact")),
        std::make_pair(EngineType::CONTAINS, static_cast<std::string>("contains")),
        std::make_pair(EngineType::STARTSWITH, static_cast<std::string>("startswith")),
};

static const EnumConverter<EngineType> engine_type_converter(engine_type_map);

/**
 * Operator that filters a feature collection based on a textual attribute
 *
 * Parameters:
 * - name: the name of the attribute
 * - engine: the type of processing the search string
 * - searchString: the search string
 */
class TextualAttributeFilterOperator : public GenericOperator {
public:
    TextualAttributeFilterOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);

    ~TextualAttributeFilterOperator() override;

#ifndef MAPPING_OPERATOR_STUBS

    auto getPointCollection(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<PointCollection> override;

    auto getLineCollection(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<LineCollection> override;

    auto getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<PolygonCollection> override;

#endif
protected:
    auto writeSemanticParameters(std::ostringstream &stream) -> void override;

private:
    std::string name;
    EngineType engine;
    std::string search_string;
};

TextualAttributeFilterOperator::TextualAttributeFilterOperator(int sourcecounts[], GenericOperator *sources[],
                                                               Json::Value &params) : GenericOperator(sourcecounts,
                                                                                                      sources) {
    assumeSources(1);

    name = params.get("name", "").asString();
    engine = engine_type_converter.from_json(params, "engine");
    search_string = params.get("searchString", "").asString();
}

TextualAttributeFilterOperator::~TextualAttributeFilterOperator() = default;

REGISTER_OPERATOR(TextualAttributeFilterOperator, "textual_attribute_filter");

void TextualAttributeFilterOperator::writeSemanticParameters(std::ostringstream &stream) {
    stream << "{";
    stream << R"("name": ")" << name << "\","
           << R"("engine": ")" << engine_type_converter.to_string(engine) << "\","
           << R"("searchString": ")" << search_string << "\"";
    stream << "}";
}

#ifndef MAPPING_OPERATOR_STUBS

auto filter(const SimpleFeatureCollection &collection, const std::string &name, const EngineType &engine_type,
            const std::string &search_string) -> std::vector<bool> {
    size_t count = collection.getFeatureCount();

    std::vector<bool> keep;
    keep.reserve(count);

    auto &attributes = collection.feature_attributes.textual(name);

    std::function<bool (const std::string&)> filter_function;
    switch (engine_type) {
        case EngineType::EXACT:
            filter_function = [search_string](const std::string &s) { return s == search_string; };
            break;
        case EngineType::CONTAINS:
            filter_function = [search_string](const std::string &s) { return s.find(search_string) != std::string::npos; };
            break;
        case EngineType::STARTSWITH:
            filter_function = [search_string](const std::string &s) { return s.find(search_string) == 0; };
            break;
    }

    for (size_t i = 0; i < count; i++) {
        const std::string &value = attributes.get(i);
        keep.push_back(filter_function(value));
    }

    return keep;
}

std::unique_ptr<PointCollection>
TextualAttributeFilterOperator::getPointCollection(const QueryRectangle &rect, const QueryTools &tools) {
    auto points = getPointCollectionFromSource(0, rect, tools);
    auto keep = filter(*points, name, engine, search_string);
    return points->filter(keep);
}

std::unique_ptr<LineCollection>
TextualAttributeFilterOperator::getLineCollection(const QueryRectangle &rect, const QueryTools &tools) {
    auto lines = getLineCollectionFromSource(0, rect, tools);
    auto keep = filter(*lines, name, engine, search_string);
    return lines->filter(keep);
}

std::unique_ptr<PolygonCollection>
TextualAttributeFilterOperator::getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) {
    auto polygons = getPolygonCollectionFromSource(0, rect, tools);
    auto keep = filter(*polygons, name, engine, search_string);
    return polygons->filter(keep);
}

#endif
