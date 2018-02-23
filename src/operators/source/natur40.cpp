#include <algorithm>
#include <pqxx/pqxx>
#include <json/json.h>
#include <cassert>
#include <numeric>

#include "operators/source/natur40.h"
#include "util/configuration.h"

const std::string LAT_COLUMN_NAME{"latitude"}; /* NOLINT */
const std::string LON_COLUMN_NAME{"longitude"}; /* NOLINT */
const std::string TIME_START_COLUMN_NAME{"time_start"}; /* NOLINT */
const std::string TIME_END_COLUMN_NAME{"time_end"}; /* NOLINT */
const std::string NODE_COLUMN_NAME{"node"}; /* NOLINT */

Natur40SourceOperator::Natur40SourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params)
        : GenericOperator(sourcecounts, sources) {
    assumeSources(0);

    auto connection_string = Configuration::get("operators.pgpointsource.dbcredentials");
    this->connection = make_unique<pqxx::connection>(connection_string);

    auto sensor_types = params.get("sensorTypes", Json::Value(Json::arrayValue));
    this->sensor_types.reserve(sensor_types.size() + 1);
    for (const auto &sensor_type_field : sensor_types) {
        const auto sensor_type = sensor_type_field.asString();
        const bool only_allowed_types = std::all_of(sensor_type.begin(), sensor_type.end(), [](char c) {
            return std::isalpha(c) || c == '_';
        });
        if (!only_allowed_types) {
            throw OperatorException {
                    "natur40_source: sensor types are only allowed to have alphanumerical values or underscore"
            };
        }
        this->sensor_types.push_back(sensor_type);
    }

    // add locations if missing
    if (std::none_of(this->sensor_types.begin(), this->sensor_types.end(),
                     [&](const std::string &t) { return t == "locations"; })) {
        this->sensor_types.emplace_back("locations");
    }
}

Natur40SourceOperator::~Natur40SourceOperator() = default;

REGISTER_OPERATOR(Natur40SourceOperator, "natur40_source");

void Natur40SourceOperator::writeSemanticParameters(std::ostringstream &stream) {
    Json::Value semantic_parameters{Json::objectValue};
    semantic_parameters["sensorTypes"] = Json::Value{Json::arrayValue};
    for (const auto &sensor_type : this->sensor_types) {
        semantic_parameters["sensorTypes"].append(sensor_type);
    }

    Json::FastWriter writer;
    stream << writer.write(semantic_parameters);
}

auto Natur40SourceOperator::getPointCollection(const QueryRectangle &rect,
                                               const QueryTools &tools) -> std::unique_ptr<PointCollection> {

    if (rect.crsId != CrsId::wgs84()) {
        throw OperatorException {"natur40_source: Must not load points in a projection other than wgs 84"};
    }

    std::stringstream sql;

    Natur40SourceOperator::add_query(
            sql,
            this->sensor_types,
            [&](const std::string &sensor_type) {
                return this->get_columns_for_sensor_type(sensor_type);
            },
            rect.t1, rect.t2
    );

    printf("%s", sql.str().c_str());

    pqxx::work transaction(*this->connection, "natur40_query");
    pqxx::result query_result = transaction.exec(sql.str());

    auto point_collection = make_unique<PointCollection>(rect);

    pqxx::tuple::size_type LAT_COLUMN = 0;
    pqxx::tuple::size_type LON_COLUMN = 0;
    pqxx::tuple::size_type TIME_START_COLUMN = 0;
    pqxx::tuple::size_type TIME_END_COLUMN = 0;
    pqxx::tuple::size_type NODE_COLUMN = 0;

    auto column_count = query_result.columns();
    for (pqxx::tuple::size_type c = 0; c < column_count; ++c) {
        auto column_name = query_result.column_name(c);

        if (column_name == LAT_COLUMN_NAME) {
            LAT_COLUMN = c;
        } else if (column_name == LON_COLUMN_NAME) {
            LON_COLUMN = c;
        } else if (column_name == TIME_START_COLUMN_NAME) {
            TIME_START_COLUMN = c;
        } else if (column_name == TIME_END_COLUMN_NAME) {
            TIME_END_COLUMN = c;
        } else if (column_name == NODE_COLUMN_NAME) {
            NODE_COLUMN = c;
            point_collection->feature_attributes.addTextualAttribute(column_name, Unit::unknown());
            point_collection->feature_attributes.textual(NODE_COLUMN_NAME).reserve(query_result.size());
        } else {
            point_collection->feature_attributes.addNumericAttribute(column_name, Unit::unknown());
            point_collection->feature_attributes.numeric(column_name).reserve(query_result.size());
        }
    }

    std::array<pqxx::tuple::size_type, 5> reserved_columns{LAT_COLUMN, LON_COLUMN,
                                                           TIME_START_COLUMN, TIME_END_COLUMN,
                                                           NODE_COLUMN};
    point_collection->time.reserve(query_result.size());
    point_collection->coordinates.reserve(query_result.size());

    for (const auto &row : query_result) {
        if (row[LAT_COLUMN].is_null() || row[LON_COLUMN].is_null()) {
            continue; // skip row with empty geo information
        }

        auto lat = row[LAT_COLUMN].as<double>();
        auto lon = row[LON_COLUMN].as<double>();

        // ensure that dates are within mapping's begin and end of time
        auto time_start = std::max(row[TIME_START_COLUMN].as<double>(), rect.beginning_of_time());
        auto time_end = std::min(row[TIME_END_COLUMN].as<double>(), rect.end_of_time());

        // fixes instants
        // TODO: think about time semantics
        if (time_start == time_end) {
            time_end += rect.epsilon();
        }

        size_t feature_id = point_collection->addSinglePointFeature(Coordinate {lon, lat});

        point_collection->time.emplace_back(time_start, time_end);

        point_collection->feature_attributes.textual(NODE_COLUMN_NAME).set(feature_id,
                                                                           row[NODE_COLUMN].as<std::string>());

        for (pqxx::result::tuple::size_type c = 0; c < column_count; ++c) {
            if (std::none_of(reserved_columns.begin(), reserved_columns.end(),
                             [&](const pqxx::result::tuple::size_type r) { return c == r; })) {
                auto column_name = query_result.column_name(c);
                // convert NULL values to NaN
                double value = row[c].is_null() ? std::numeric_limits<double>::quiet_NaN() : row[c].as<double>();
                point_collection->feature_attributes.numeric(column_name).set(feature_id, value);
            }
        }
    }

    // TODO: capture I/O cost somehow

    return point_collection;
}

auto Natur40SourceOperator::add_view(std::stringstream &stream,
                                     const std::string &table,
                                     const std::vector<std::string> &columns,
                                     double time_start,
                                     double time_end) -> void {
    assert(!columns.empty());

    std::map<std::string, std::string> replacements{
            {"@table@",        table},
            {"@columns@",      std::accumulate(
                    std::next(columns.begin()), // start with second element
                    columns.end(),
                    columns[0], // initialize with first element
                    [](std::string a, std::string b) {
                        return a + ", " + b;
                    })
            },
            {"@null_columns@", std::accumulate(
                    std::next(columns.begin()), // start with second element
                    columns.end(),
                    "NULL AS " + columns[0], // initialize with first element
                    [](std::string a, std::string b) {
                        return a + ", NULL AS " + b;
                    })
            },
            {"@time_start@",   "to_timestamp(" + std::to_string(time_start) + ")"},
            {"@time_end@",     "to_timestamp(" + std::to_string(time_end) + ")"},
    };

    const std::string query_template = R"(
        @table@ AS (
            SELECT * FROM (
                SELECT
                    node,
                    "time" AS time_start,
                    (
                        SELECT COALESCE(MIN("time"), 'infinity')
                        FROM @table@ o
                        WHERE o.node = t.node AND o."time" > t."time"
                    ) AS time_end,
                    @columns@
                FROM (
                    SELECT node, "time", @columns@ FROM @table@
                    UNION
                    SELECT node, '-infinity' as "time", @null_columns@
                    FROM @table@
                    GROUP BY node
                ) t
            ) r
            WHERE (time_start, time_end) OVERLAPS (@time_start@, @time_end@)
        )
    )";

    parse_query(stream, query_template, replacements);
}

auto Natur40SourceOperator::parse_query(std::stringstream &stream,
                                        const std::string &query_template,
                                        const std::map<std::string, std::string> &replacements) -> void {
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
                        stream << c;
                    }
                }
                break;
            case BUFFER:
                buffer << c;
                if (c == '@') {
                    state = CHAR;
                    stream << replacements.at(buffer.str());
                    buffer.str("");
                }
                break;
        }
        last_char = c;
    }
}

auto
Natur40SourceOperator::get_columns_for_sensor_type(const std::string &sensor_type) -> const std::vector<std::string> {
    std::vector<std::string> columns;

    // TODO: grab from postgres?

    if (sensor_type == "locations") {
        columns.emplace_back("longitude");
        columns.emplace_back("latitude");
        columns.emplace_back("altitude");
    } else if (sensor_type == "temperatures") {
        columns.emplace_back("temperature");
    } else if (sensor_type == "images") {
        columns.emplace_back("file");
    } else {
        throw OperatorException {"natur40_source: Unkown sensor type `" + sensor_type + "`"};
    }

    assert(!columns.empty());

    return columns;
}

auto Natur40SourceOperator::add_main_query(std::stringstream &stream,
                                           const std::vector<std::string> &tables,
                                           const std::vector<std::string> &columns,
                                           double time_start,
                                           double time_end) -> void {
    assert(!tables.empty());
    assert(!columns.empty());

    std::stringstream tables_time_overlaps;
    for (size_t i = 0; i < tables.size(); ++i) {
        for (size_t j = i + 1; j < tables.size(); ++j) {
            tables_time_overlaps << " AND (";
            tables_time_overlaps << "(" << tables[i] << ".time_start, " << tables[i] << ".time_end)";
            tables_time_overlaps << " OVERLAPS ";
            tables_time_overlaps << "(" << tables[j] << ".time_start, " << tables[j] << ".time_end)";
            tables_time_overlaps << " OR ";
            tables_time_overlaps << tables[i] << ".node IS NULL OR " << tables[j] << ".node IS NULL";
            tables_time_overlaps << ") ";
        }
    }

    std::map<std::string, std::string> replacements{
            {"@tables_time_start@",    std::accumulate(
                    std::next(tables.begin()), // start with second element
                    tables.end(),
                    tables[0] + ".time_start", // initialize with first element
                    [](std::string a, std::string b) {
                        return a + ", " + b + ".time_start";
                    })
            },
            {"@tables_time_end@",      std::accumulate(
                    std::next(tables.begin()), // start with second element
                    tables.end(),
                    tables[0] + ".time_end", // initialize with first element
                    [](std::string a, std::string b) {
                        return a + ", " + b + ".time_end";
                    })
            },
            {"@outer_join_tables@",    std::accumulate(
                    std::next(tables.begin()), // start with second element
                    tables.end(),
                    tables[0], // initialize with first element
                    [](std::string a, std::string b) {
                        return a + " FULL OUTER JOIN " + b + " USING(node)";
                    })
            },
            {"@tables_time_overlaps@", tables_time_overlaps.str()},
            {"@columns@",              std::accumulate(
                    std::next(columns.begin()), // start with second element
                    columns.end(),
                    columns[0], // initialize with first element
                    [](std::string a, std::string b) {
                        return a + ", " + b;
                    })
            },
            {"@is_null_columns@",      std::accumulate(
                    std::next(columns.begin()), // start with second element
                    columns.end(),
                    columns[0] + " IS NULL", // initialize with first element
                    [](std::string a, std::string b) {
                        return a + " AND " + b + " IS NULL";
                    })
            },
            {"@time_start@",           "to_timestamp(" + std::to_string(time_start) + ")"},
            {"@time_end@",             "to_timestamp(" + std::to_string(time_end) + ")"},
    };

    const std::string query_template = R"(
        SELECT
            node,
            GREATEST(@tables_time_start@) as time_start,
            LEAST(@tables_time_end@) as time_end,
            @columns@
        FROM
            @outer_join_tables@
        WHERE
            NOT (@is_null_columns@)
            @tables_time_overlaps@
            AND (GREATEST(@tables_time_start@), LEAST(@tables_time_end@)) OVERLAPS (@time_start@, @time_end@)
    )";

    parse_query(stream, query_template, replacements);
}

auto Natur40SourceOperator::add_query(std::stringstream &stream,
                                      const std::vector<std::string> &tables,
                                      std::function<const std::vector<std::string>(const std::string &)> column_getter,
                                      double time_start,
                                      double time_end) -> void {
    stream << "WITH ";

    std::vector<std::string> columns;

    for (const auto &table : tables) {
        auto table_columns = column_getter(table);
        columns.insert(columns.end(), table_columns.begin(), table_columns.end());

        Natur40SourceOperator::add_view(stream, table, table_columns, time_start, time_end);
        stream << ", ";
    }
    stream.seekp(-2, std::ios_base::end); // remove trailing comma

    Natur40SourceOperator::add_main_query(stream, tables, columns, time_start, time_end);
}
