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

        auto get_columns_for_sensor_type(const std::string &sensor_type) -> const std::vector<std::string>;

    public:
        // TODO: must be private, but it is untestable then

        /**
         * Create view creation string for a event table.
         *
         * @param stream
         * @param table
         * @param columns
         * @param time_start
         * @param time_end
         */
        static auto add_view(std::stringstream &stream,
                             const std::string &table,
                             const std::vector<std::string> &columns,
                             double time_start,
                             double time_end) -> void;

        /**
         * Create the query for retrieving the sensor events.
         *
         * @param stream
         * @param tables
         * @param columns
         * @param time_start
         * @param time_end
         */
        static auto add_main_query(std::stringstream &stream,
                                   const std::vector<std::string> &tables,
                                   const std::vector<std::string> &columns,
                                   double time_start,
                                   double time_end) -> void;

        /**
         * Replace markers in a query template and output it into a string stream
         * @param stream
         * @param query_template
         * @param replacements
         */
        static void parse_query(std::stringstream &stream,
                                const std::string &query_template,
                                const std::map<std::string, std::string> &replacements);

        /**
         * Create the whole query
         * @param stream
         * @param tables
         * @param column_getter
         * @param time_start
         * @param time_end
         */
        static auto add_query(std::stringstream &stream,
                              const std::vector<std::string> &tables,
                              std::function<const std::vector<std::string>(const std::string &)>,
                              double time_start,
                              double time_end) -> void;
};
