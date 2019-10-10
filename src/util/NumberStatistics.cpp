#include <cmath>
#include "NumberStatistics.h"

auto NumberStatistics::add(double value) -> void {
    if (std::isnan(value)) {
        this->value_nan_count += 1;
        return;
    }

    this->min_value = std::fmin(this->min_value, value);
    this->max_value = std::fmax(this->max_value, value);

    // Welford's algorithm
    this->value_count += 1;
    double delta = value - this->mean_value;
    this->mean_value += delta / this->value_count;
    double delta2 = value - this->mean_value;
    this->M2 += delta * delta2;
}

auto NumberStatistics::var() const -> double {
    if (this->value_count > 0) {
        return this->M2 / this->value_count;
    } else {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

auto NumberStatistics::std_dev() const -> double {
    if (this->value_count > 1) {
        return std::sqrt(this->M2 / (this->value_count));
    } else {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

auto NumberStatistics::sample_std_dev() const -> double {
    if (this->value_count > 1) {
        return std::sqrt(this->M2 / (this->value_count - 1.0));
    } else {
        return std::numeric_limits<double>::quiet_NaN();
    }
}


