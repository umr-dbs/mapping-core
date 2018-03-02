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

        using table_t = std::string;
        using column_t = std::string;

        /**
         * Table row with extracted information
         */
        class Row {
            public:
                Row(const pqxx::tuple &tuple,
                    const std::vector<column_t> &numeric_columns,
                    const std::vector<column_t> &textual_columns,
                    const TemporalReference &tref);

                Row(std::string node,
                    double time_start,
                    double time_end,
                    std::map<std::string, double> numeric_values,
                    std::map<std::string, std::string> textual_values);

                const std::string node;
                const double time_start;
                const double time_end;
                const std::map<std::string, double> numeric_values;
                const std::map<std::string, std::string> textual_values;
        };

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
         * Retrieve numeric columns for table
         * @param table_name
         * @return
         */
        auto get_numeric_columns_for_table(const std::string &table_name) -> const std::vector<std::string>;

        /**
         * Retrieve textual columns for table
         * @param table_name
         * @return
         */
        auto get_textual_columns_for_table(const std::string &table_name) -> const std::vector<std::string>;

    public:
        // TODO: must be private, but it is untestable then

        /**
         * Create a sorted table query for a table and its columns.
         *
         * @param stream
         * @param table
         * @param numeric_columns
         * @param time_start
         * @param time_end
         */
        static auto table_query(const std::string &table,
                                const std::vector<std::string> &numeric_columns,
                                const std::vector<std::string> &textual_columns,
                                double time_start, double time_end) -> std::string;

        /**
         * Replace markers in a query template and output it into a string stream.
         *
         * @param stream
         * @param query_template
         * @param replacements
         */
        static auto parse_query(const std::string &query_template,
                                const std::map<std::string, std::string> &replacements) -> std::string;

        /**
         * Merge multiple row vectors
         * @param results
         * @param f
         */
        static auto
        merge_results(std::map<table_t, std::vector<Row>> results, const std::function<void(const Row &)> &f) -> void;

        /**
         * Create a feature collection out of query results
         * @param results
         * @param numeric_columns
         * @param textual_columns
         * @param rect
         * @return
         */
        static auto create_feature_collection(const std::map<table_t, std::vector<Row>> &results,
                                              const std::vector<column_t> &numeric_columns,
                                              const std::vector<column_t> &textual_columns,
                                              const QueryRectangle &rect) -> std::unique_ptr<PointCollection>;
};
