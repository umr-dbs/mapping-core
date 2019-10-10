#ifndef MAPPING_CORE_NUMBERSTATISTICS_H
#define MAPPING_CORE_NUMBERSTATISTICS_H

#include <limits>
#include <cmath>

/**
 * This class provides some basic number statistics.
 * The algorithm runs in linear time.
 */
class NumberStatistics {
    public:
        auto add(double value) -> void;

        auto count() const -> std::size_t { return this->value_count; };

        auto nan_count() const -> std::size_t { return this->value_nan_count; };

        auto min() const -> double { return this->min_value; };

        auto max() const -> double { return this->max_value; };

        auto mean() const -> double { return this->mean_value; };

        auto var() const -> double;

        auto std_dev() const -> double;

        auto sample_std_dev() const -> double;

    private:
        double min_value = std::numeric_limits<double>::max();
        double max_value = -std::numeric_limits<double>::max();
        std::size_t value_count = 0;
        std::size_t value_nan_count = 0;
        double mean_value = 0;
        double M2 = 0;
};


#endif //MAPPING_CORE_NUMBERSTATISTICS_H
