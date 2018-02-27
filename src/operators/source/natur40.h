#pragma once

#include <pqxx/pqxx>
#include <json/json.h>

#include "operators/operator.h"
#include "datatypes/pointcollection.h"

/**
 * Operator that retrieves Natur 4.0 sensor data from a postgres database
 *
 * Parameters:
 * - sensorTypes: an array of sensor types (string)
 *      note: `locations` is set implicitly
 */
class Natur40SourceOperator : public GenericOperator {
    public:
        Natur40SourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);

        ~Natur40SourceOperator() override;

        auto getPointCollection(const QueryRectangle &rect,
                                const QueryTools &tools) -> std::unique_ptr<PointCollection> override;

    protected:
        auto writeSemanticParameters(std::ostringstream &stream) -> void override;

    private:
        std::unique_ptr<pqxx::connection> connection;
        std::vector<std::string> sensor_types;

        /**
         * Retrieve the table name for a sensor type
         * @param sensor_type
         * @return
         */
        auto get_table_name(const std::string &sensor_type) -> const std::string;

        /**
         * Retrieve columns for table
         * @param table_name
         * @return
         */
        auto get_columns_for_table(const std::string &table_name) -> const std::vector<std::string>;

    public:
        // TODO: must be private, but it is untestable then

        /**
         * Create a sorted table query for a table and its columns.
         *
         * @param stream
         * @param table
         * @param columns
         * @param time_start
         * @param time_end
         */
        static auto table_query(const std::string &table,
                                const std::vector<std::string> &columns,
                                double time_start,
                                double time_end) -> std::string;

        /**
         * Replace markers in a query template and output it into a string stream.
         *
         * @param stream
         * @param query_template
         * @param replacements
         */
        static auto parse_query(const std::string &query_template,
                                const std::map<std::string, std::string> &replacements) -> std::string;
};
