#include <util/exceptions.h>
#include <datatypes/plots/histogram.h>
#include <cmath>
#include <numeric>
#include <json/json.h>

Histogram::Histogram(unsigned long number_of_buckets, double min, double max)
        : Histogram(number_of_buckets, min, max, "") {
}

Histogram::Histogram(unsigned long number_of_buckets, double min, double max, const Unit &unit)
        : Histogram(number_of_buckets, min, max, compute_unit_string(unit)) {
}

Histogram::Histogram(unsigned long number_of_buckets, double min, double max, std::string unit_string)
        : counts(number_of_buckets), nodata_count(0), min(min), max(max), unit(std::move(unit_string)) {
    if (!std::isfinite(min)) {
        throw ArgumentException("Histogram: min is not finite");
    } else if (!std::isfinite(max)) {
        throw ArgumentException("Histogram: max is not finite");
    } else if (min == max && number_of_buckets > 1) {
        throw ArgumentException("Histogram: number_of_buckets must be 1 if min = max");
    } else if (min > max) {
        throw ArgumentException(concat("Histogram: min > max (", min, " > ", max, ")"));
    }
}

Histogram::~Histogram() = default;

auto Histogram::compute_unit_string(const Unit &unit) -> std::string {
    static const Unit unknown_unit = Unit::unknown();
    
    std::stringstream unit_string;
    if (unit.getMeasurement() != unknown_unit.getMeasurement()) {
        unit_string << unit.getMeasurement();
        if (!unit.isClassification() && unit.getUnit() != unknown_unit.getUnit()) {
            unit_string << " in " << unit.getUnit();
        }
    }
    return unit_string.str();
}

void Histogram::inc(double value) {
    if (value < min || value > max) {
        incNoData();
        return;
    }

    counts[calculateBucketForValue(value)]++;
}

int Histogram::calculateBucketForValue(double value) {
    if (max > min) {
        auto bucket = static_cast<int>(std::floor(((value - min) / (max - min)) * counts.size()));
        bucket = std::min((int) counts.size() - 1, std::max(0, bucket));
        return bucket;
    } else {
        return 0; // `number_of_buckets` is 1 if `min = max`
    }
}

double Histogram::calculateBucketLowerBorder(int bucket) {
    return (bucket * ((max - min) / counts.size())) + min;
}

void Histogram::incNoData() {
    nodata_count++;
}

int Histogram::getValidDataCount() {
    return std::accumulate(counts.begin(), counts.end(), 0);
}

void Histogram::addMarker(double bucket, const std::string &label) {
    markers.emplace_back(bucket, label);
}

auto Histogram::toJSON() const -> const std::string {
    Json::Value metadata(Json::ValueType::objectValue);
    metadata["min"] = min;
    metadata["max"] = max;
    metadata["nodata"] = nodata_count;
    metadata["numberOfBuckets"] = counts.size();
    metadata["unit"] = unit;

    Json::Value data(Json::ValueType::arrayValue);
    for (const int count : counts) {
        data.append(count);
    }

    Json::Value lines(Json::ValueType::arrayValue);
    for (const auto &marker : markers) {
        Json::Value line(Json::ValueType::objectValue);
        line["name"] = marker.second;
        line["pos"] = marker.first;
        lines.append(line);
    }

    Json::Value json(Json::ValueType::objectValue);
    json["type"] = "histogram";
    json["metadata"] = metadata;
    json["data"] = data;
    if (!lines.empty()) json["lines"] = lines;

    Json::FastWriter writer;
    return writer.write(json);
}

std::unique_ptr<GenericPlot> Histogram::clone() const {
    auto copy = std::make_unique<Histogram>(counts.size(), min, max, unit);

    copy->nodata_count = nodata_count;
    copy->markers = markers;
    copy->counts = counts;

    return std::unique_ptr<GenericPlot>(copy.release());
}

Histogram::Histogram(BinaryReadBuffer &buffer) {
    buffer.read(&counts);
    buffer.read(&nodata_count);
    buffer.read(&min);
    buffer.read(&max);
    buffer.read(&unit);

    auto count = buffer.read<size_t>();
    for (size_t i = 0; i < count; i++) {
        auto p1 = buffer.read<double>();
        std::string p2 = buffer.read<std::string>();
        markers.emplace_back(p1, p2);
    }
}

void Histogram::serialize(BinaryWriteBuffer &buffer, bool is_persistent_memory) const {
    buffer << Type::Histogram;

    buffer << counts;
    buffer << nodata_count;
    buffer << min;
    buffer << max;
    buffer << unit;

    size_t count = markers.size();
    buffer.write(count);
    for (auto &e : markers) {
        buffer.write(e.first);
        buffer.write(e.second);
    }
}
