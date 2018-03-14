#include <algorithm>
#include <pqxx/pqxx>
#include <json/json.h>
#include <cassert>
#include <numeric>
#include <future>
#include <queue>

#include "util/timeparser.h"
#include "operators/source/natur40.h"
#include "util/configuration.h"

#define NATUR40_DEBUG 0

Natur40SourceOperator::Natur40SourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params)
        : GenericOperator(sourcecounts, sources) {
    assumeSources(0);

    auto connection_string = Configuration::get("operators.pgpointsource.dbcredentials");
    this->connection = make_unique<pqxx::connection>(connection_string);

    auto sensor_types = params.get("sensorTypes", Json::Value(Json::arrayValue));
    this->sensor_types.reserve(sensor_types.size() + 1);
    for (const auto &sensor_type_field : sensor_types) {
        const auto sensor_type = sensor_type_field.asString();

        // check for legal characters with respect to SQL queries
        const bool only_allowed_types = std::all_of(sensor_type.begin(), sensor_type.end(), [](char c) {
            return std::isalnum(c) || c == '_';
        });
        if (!only_allowed_types) {
            throw OperatorException {
                    "natur40_source: sensor types are only allowed to have alphanumerical values or underscores"
            };
        }

        // make sensor type lowercase
        std::string sensor_type_lowercase;
        std::transform(std::begin(sensor_type), std::end(sensor_type), std::begin(sensor_type_lowercase), ::tolower);

        this->sensor_types.push_back(sensor_type_lowercase);
    }

    // add locations if missing
    if (std::none_of(this->sensor_types.begin(), this->sensor_types.end(),
                     [&](const std::string &t) { return t == "location"; })) {
        this->sensor_types.emplace_back("location");
    }
}

Natur40SourceOperator::~Natur40SourceOperator() = default;

REGISTER_OPERATOR(Natur40SourceOperator, "natur40_source"); /* NOLINT */

auto Natur40SourceOperator::writeSemanticParameters(std::ostringstream &stream) -> void {
    Json::Value semantic_parameters{Json::objectValue};
    semantic_parameters["sensorTypes"] = Json::Value{Json::arrayValue};
    for (const auto &sensor_type : this->sensor_types) {
        semantic_parameters["sensorTypes"].append(sensor_type);
    }

    Json::FastWriter writer;
    stream << writer.write(semantic_parameters);
}

const std::string LAT_COLUMN_NAME{"latitude"}; /* NOLINT */
const std::string LON_COLUMN_NAME{"longitude"}; /* NOLINT */
const std::string TIME_START_COLUMN_NAME{"time_start"}; /* NOLINT */
const std::string TIME_END_COLUMN_NAME{"time_end"}; /* NOLINT */
const std::string NODE_COLUMN_NAME{"node"}; /* NOLINT */

Natur40SourceOperator::Row::Row(const pqxx::tuple &tuple,
                                const std::vector<column_t> &numeric_columns,
                                const std::vector<column_t> &textual_columns,
                                const TemporalReference &tref) :
        node(tuple[NODE_COLUMN_NAME].as<std::string>()),
        time_start(tuple[TIME_START_COLUMN_NAME].as<double>()),
        time_end(tuple[TIME_END_COLUMN_NAME].as<double>()) {
    if (this->time_start < tref.beginning_of_time()) {
        const_cast<double &>(this->time_start) = tref.beginning_of_time();
    }
    if (this->time_end > tref.end_of_time()) {
        const_cast<double &>(this->time_end) = tref.end_of_time();
    }

    auto &numeric_map = const_cast<std::map<std::string, double> &>(this->numeric_values);
    for (const auto &numeric_column : numeric_columns) {
        numeric_map[numeric_column] = tuple[numeric_column].is_null()
                                      ? std::numeric_limits<double>::quiet_NaN()
                                      : tuple[numeric_column].as<double>();
    }
    auto &textual_map = const_cast<std::map<std::string, std::string> &>(this->textual_values);
    for (const auto &textual_column : textual_columns) {
        textual_map[textual_column] = tuple[textual_column].is_null()
                                      ? ""
                                      : tuple[textual_column].as<std::string>();
    }
}

Natur40SourceOperator::Row::Row(const std::string node,
                                double time_start,
                                double time_end,
                                const std::map<std::string, double> numeric_values,
                                const std::map<std::string, std::string> textual_values) :
        node(node),
        time_start(time_start), time_end(time_end),
        numeric_values(numeric_values),
        textual_values(textual_values) {}

using Row = Natur40SourceOperator::Row;
using table_t = Natur40SourceOperator::table_t;
using column_t = Natur40SourceOperator::column_t;

/**
 * Wrapper Class for an `Row` iterator and its end.
 */
class IterWrap {
        using container_t = std::vector<Row>;
        using iter_t = container_t::const_iterator;

    public:
        explicit IterWrap(container_t &container) : current(std::begin(container)), end(std::end(container)) {}

        auto node() const -> const std::string & {
            return this->current->node;
        }

        auto time() const -> double {
            return this->current->time_start;
        }

        auto row() const -> Row {
            return *this->current;
        }

        auto isEnd() const -> bool {
            return this->current == this->end;
        }

        auto operator++() -> void {
            ++this->current;
        }

    private:
        iter_t current;
        iter_t end;

};

/**
 * Reverse Comparator for using a Max Heap as a Min Heap
 */
class IterWrapComparator {
    public:
        auto operator()(const IterWrap &a, const IterWrap &b) -> int {
            if (a.isEnd()) {
                return true; // use b
            } else if (b.isEnd()) {
                return false; // use a
            }

            if (a.node() < b.node()) {
                return false; // use a
            } else if (a.node() > b.node()) {
                return true; // use b
            }

            return a.time() >= b.time();
        }
};

/**
 * Method that merges results of multiple containers and calls the function `f` in ordered manner.
 * @param results
 * @param f
 */
auto Natur40SourceOperator::merge_results(std::map<table_t, std::vector<Row>> results,
                                          const std::function<void(const Row &)> &f) -> void {
    using iter = std::vector<Row>::const_iterator;

    std::priority_queue<IterWrap, std::vector<IterWrap>, IterWrapComparator> heap;
    for (auto &result :            results) {
        heap.emplace(result.second);
    }

    while (!heap.empty()) {
        auto result = heap.top();
        heap.pop();

        if (result.isEnd()) {
            continue;
        }

        f(result.row());

        ++result;
        heap.push(result);
    }

}

/**
 * Add data to the point collection.
 * Filteres out invalid geo and time information
 * @param collection
 * @param node
 * @param time_start
 * @param time_end
 * @param numeric_values
 * @param qrect
 */
auto add_to_collection(PointCollection &collection, std::string &node,
                       double time_start, double time_end,
                       std::map<column_t, double> &numeric_values,
                       std::map<column_t, std::string> &textual_values,
                       const QueryRectangle &qrect) -> void {
#if NATUR40_DEBUG
    {
        TemporalReference tref{TIMETYPE_UNIX};
        std::ostringstream v1;
        v1 << "{";
        for (const auto &value : numeric_values) {
            v1 << value.first << ": " << value.second << ", ";
        }
        v1 << "}";
        std::ostringstream v2;
        v2 << "{";
        for (const auto &value : textual_values) {
            v2 << value.first << ": " << value.second << ", ";
        }
        v2 << "}";
        printf("ENTRY: %s %s %s %s %s\n",
               node.c_str(),
               tref.toIsoString(time_start).c_str(), tref.toIsoString(time_end).c_str(),
               v1.str().c_str(),
               v2.str().c_str()
        );
    }
#endif

    double longitude = numeric_values[LON_COLUMN_NAME];
    double latitude = numeric_values[LAT_COLUMN_NAME];

    if (std::isnan(longitude) || std::isnan(latitude)) {
#if NATUR40_DEBUG
        printf("REJECTED! geo\n\n");
#endif
        return; // geo information is missing
    }
    if (time_start > qrect.t2 || time_end <= qrect.t1) {
#if NATUR40_DEBUG
        printf("REJECTED! time\n\n");
#endif
        return; // time is out of bounds
    }

#if NATUR40_DEBUG
    printf("ACCEPTED!\n\n");
#endif

    auto feature_id = collection.addSinglePointFeature(Coordinate(longitude, latitude));

    collection.time.emplace_back(time_start, time_end);

    collection.feature_attributes.textual(NODE_COLUMN_NAME).set(feature_id, node);

    for (const auto &value : numeric_values) {
        if (value.first != LON_COLUMN_NAME && value.first != LAT_COLUMN_NAME) {
            collection.feature_attributes.numeric(value.first).set(feature_id, value.second);
        }
    }

    for (const auto &value : textual_values) {
        collection.feature_attributes.textual(value.first).set(feature_id, value.second);
    }
}

auto reset_numeric_map(std::map<column_t, double> &map) -> void {
    for (auto &kv : map) {
        kv.second = std::numeric_limits<double>::quiet_NaN();
    }
}

auto reset_textual_map(std::map<column_t, std::string> &map) -> void {
    for (auto &kv : map) {
        kv.second = "";
    }
}

auto Natur40SourceOperator::getPointCollection(const QueryRectangle &rect,
                                               const QueryTools &tools) -> std::unique_ptr<PointCollection> {

    if (rect.crsId != CrsId::wgs84()) {
        throw OperatorException {"natur40_source: Must not load points in a projection other than wgs 84"};
    }

    pqxx::read_transaction transaction{*this->connection, "natur40_query"};
    transaction.exec("SET TIME ZONE 0"); // set TIME ZONE to UTC

    // map sensor types to table names
    std::vector<table_t> tables;
    for (const auto &sensor_type : this->sensor_types) {
        tables.emplace_back(this->get_table_name(sensor_type));
    }

    std::map<table_t, std::vector<Row>> results;
    std::vector<column_t> numeric_columns;
    std::vector<column_t> textual_columns;

    // create query
    for (const auto &table : tables) {
        const auto table_numeric_columns = get_numeric_columns_for_table(table);
        const auto table_textual_columns = get_textual_columns_for_table(table);

        std::string query = table_query(table, table_numeric_columns, table_textual_columns, rect.t1, rect.t2);
#if NATUR40_DEBUG
        printf("%s\n\n", query.c_str());
#endif

        results[table] = std::vector<Row>{};
        pqxx::result result = transaction.exec(query);

        for (const auto &row : result) {
#if NATUR40_DEBUG
            {
                TemporalReference tref{TIMETYPE_UNIX};
                auto time_start = row[TIME_START_COLUMN_NAME].as<double>();
                auto time_end = row[TIME_END_COLUMN_NAME].as<double>();
                printf(
                        "DB: %s -> %s -> %s %s \t\t %f %f\n",
                        table.c_str(),
                        row[NODE_COLUMN_NAME].as<std::string>().c_str(),
                        tref.toIsoString(time_start > rect.beginning_of_time() ? time_start
                                                                               : rect.beginning_of_time()).c_str(),
                        tref.toIsoString(time_end < rect.end_of_time() ? time_end
                                                                       : rect.end_of_time()).c_str(),
                        time_start,
                        time_end
                );
            }
#endif
            results[table].emplace_back(row, table_numeric_columns, table_textual_columns, rect);
        }

        for (const auto &column : table_numeric_columns) {
            numeric_columns.emplace_back(column);
        }

        for (const auto &column : table_textual_columns) {
            textual_columns.emplace_back(column);
        }
    }

    return create_feature_collection(results, numeric_columns, textual_columns, rect);
}

auto Natur40SourceOperator::create_feature_collection(const std::map<table_t, std::vector<Row>> &results,
                                                      const std::vector<column_t> &numeric_columns,
                                                      const std::vector<column_t> &textual_columns,
                                                      const QueryRectangle &rect) -> std::unique_ptr<PointCollection> {
    // initialize feature collection
    auto point_collection = make_unique<PointCollection>(rect);
    point_collection->feature_attributes.addTextualAttribute(NODE_COLUMN_NAME, Unit::unknown());
    for (const auto &column : numeric_columns) {
        if (column == LON_COLUMN_NAME || column == LAT_COLUMN_NAME) {
            continue; // do not save lon/lat in numeric attributes
        }
        point_collection->feature_attributes.addNumericAttribute(column, Unit::unknown());
    }
    for (const auto &column : textual_columns) {
        point_collection->feature_attributes.addTextualAttribute(column, Unit::unknown());
    }

    // initialize row values
    std::map<column_t, double> numeric_values;
    for (const auto &column : numeric_columns) {
        numeric_values[column] = std::numeric_limits<double>::quiet_NaN();
    }
    std::map<column_t, std::__cxx11::string> textual_values;
    for (const auto &column : textual_columns) {
        textual_values[column] = "";
    }
    std::__cxx11::string current_node;
    double current_time = rect.beginning_of_time();
    double current_time_end = rect.beginning_of_time();

    merge_results(results, [&](const Row &row) {
#if NATUR40_DEBUG
        {
            TemporalReference tref{TIMETYPE_UNIX};
            std::ostringstream v1;
            v1 << "{";
            for (const auto &value : row.numeric_values) {
                v1 << value.first << ": " << value.second << ", ";
            }
            v1 << "}";
            std::ostringstream v2;
            v2 << "{";
            for (const auto &value : row.textual_values) {
                v2 << value.first << ": " << value.second << ", ";
            }
            v2 << "}";
            printf("ROW: %s %s %s %s %s\n\n",
                   row.node.c_str(),
                   tref.toIsoString(row.time_start).c_str(),
                   tref.toIsoString(row.time_end).c_str(),
                   v1.str().c_str(), v2.str().c_str()
            );
        }
#endif

        if (row.node != current_node) {
            // finish node
            add_to_collection(*point_collection,
                              current_node,
                              current_time, current_time_end,
                              numeric_values, textual_values,
                              rect);

            reset_numeric_map(numeric_values);
            reset_textual_map(textual_values);
        } else if (row.time_start > current_time) {
            add_to_collection(*point_collection,
                              current_node,
                              current_time, row.time_start,
                              numeric_values, textual_values,
                              rect);
        }

        for (const auto &value : row.numeric_values) {
            numeric_values[value.first] = value.second;
        }
        for (const auto &value : row.textual_values) {
            textual_values[value.first] = value.second;
        }

        current_node = row.node;
        current_time = row.time_start;
        current_time_end = row.time_end;
    });

    add_to_collection(*point_collection,
                      current_node,
                      current_time, current_time_end <= rect.end_of_time() ? current_time_end : rect.end_of_time(),
                      numeric_values, textual_values,
                      rect);
    return point_collection;
}

auto Natur40SourceOperator::table_query(const std::string &table,
                                        const std::vector<std::string> &numeric_columns,
                                        const std::vector<std::string> &textual_columns,
                                        double time_start, double time_end) -> std::string {
    std::vector<std::string> columns;
    std::merge(
            std::begin(numeric_columns), std::end(numeric_columns),
            std::begin(textual_columns), std::end(textual_columns),
            std::back_inserter(columns)
    );

    assert(!columns.empty());

    std::map<std::string, std::string> replacements{
            {"@table@",      table},
            {"@columns@",    std::accumulate(
                    std::next(columns.begin()), // start with second element
                    columns.end(),
                    columns[0], // initialize with first element
                    [](std::string a, std::string b) {
                        return a + ", " + b;
                    })
            },
            {"@time_start@", "to_timestamp(" + std::to_string(time_start) + ")::TIMESTAMP WITHOUT TIME ZONE"},
            {"@time_end@",   "to_timestamp(" + std::to_string(time_end) + ")::TIMESTAMP WITHOUT TIME ZONE"},
    };

    const std::string query_template = R"(
        SELECT
            CASE time_start
                WHEN '-infinity' THEN '-infinity'::FLOAT8
                ELSE EXTRACT(EPOCH FROM time_start)
            END AS time_start,
            CASE time_end
                WHEN 'infinity' THEN 'infinity'::FLOAT8
                ELSE EXTRACT(EPOCH FROM time_end)
            END AS time_end,
            node,
            @columns@
        FROM (
            SELECT
                time as time_start,
                COALESCE(
                    LEAD(time) OVER (PARTITION BY node
                                     ORDER BY time ASC
                                     ROWS BETWEEN CURRENT ROW AND 1 FOLLOWING),
                    'infinity'
                ) AS time_end,
            node,
            @columns@
            FROM @table@
        ) t
        WHERE (time_start, time_end) OVERLAPS (@time_start@, @time_end@)
        ORDER BY node, time_start ASC;
    )";

    return parse_query(query_template, replacements);
}

auto Natur40SourceOperator::parse_query(const std::string &query_template,
                                        const std::map<std::string, std::string> &replacements) -> std::string {
    std::stringstream output;

    std::stringstream buffer;
    char last_char = '\n';
    enum {
        CHAR,
        BUFFER,
    } state = CHAR;

    for (const auto c : query_template) {
        switch (state) {
            case CHAR:
                if (c == '@') {
                    buffer << c;
                    state = BUFFER;
                } else {
                    if (c != ' ' || last_char != ' ') {
                        output << c;
                    }
                }
                break;
            case BUFFER:
                buffer << c;
                if (c == '@') {
                    state = CHAR;
                    output << replacements.at(buffer.str());
                    buffer.str("");
                }
                break;
        }
        last_char = c;
    }

    return output.str();
}

auto
Natur40SourceOperator::get_table_name(const std::string &sensor_type) -> const std::string {
    // TODO: grab from postgres or file

    std::map<std::string, table_t> tables = {
            {"location",               "locations"},
            {"light",                  "lights"},
            {"pressure",               "pressures"},
            {"temperature",            "temperatures"},
            {"co2",                    "co2"},
            {"tvoc",                   "tvoc"},
            {"image",                  "images"},
            {"uv_irradiance",          "uv_irradiance"},
            {"solar_irradiance",       "solar_irradiance"},
            {"ir_surface_temperature", "ir_surface_tempe"},
            {"rh_sht",                 "rh_sht"},
            {"t_sht",                  "t_sht"},
    };

    const auto map_entry = tables.find(sensor_type);

    if (map_entry != std::end(tables)) {
        return map_entry->second;
    } else {
        throw OperatorException {"natur40_source: Unknown sensor type `" + sensor_type + "`"};
    }
}

auto
Natur40SourceOperator::get_numeric_columns_for_table(const table_t &table_name) -> const std::vector<std::string> {
    // TODO: grab from postgres or file

    std::map<std::string, std::vector<column_t>> numeric_columns = {
            {"locations",        {"longitude", "latitude", "satellites"}},
            {"lights",           {"light"}},
            {"pressures",        {"pressure"}},
            {"temperatures",     {"temperature"}},
            {"co2",              {"co2"}},
            {"tvoc",             {"tvoc"}},
            {"uv_irradiance",    {"uv_irradiance"}},
            {"solar_irradiance", {"solar_irradiance"}},
            {"ir_surface_tempe", {"ir_surface_temp"}},
            {"rh_sht",           {"rh_sht"}},
            {"t_sht",            {"t_sht"}},
    };

    const auto map_entry = numeric_columns.find(table_name);

    if (map_entry != std::end(numeric_columns)) {
        return map_entry->second;
    } else {
        return {};
    }
}

auto
Natur40SourceOperator::get_textual_columns_for_table(const std::string &table_name) -> const std::vector<std::string> {
    // TODO: grab from postgres or file

    std::map<std::string, std::vector<column_t>> textual_columns = {
            {"images", {"image"}},
    };

    const auto map_entry = textual_columns.find(table_name);

    if (map_entry != std::end(textual_columns)) {
        return map_entry->second;
    } else {
        return {};
    }
}
