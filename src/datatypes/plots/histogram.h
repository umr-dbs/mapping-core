#ifndef PLOT_HISTOGRAM_H
#define PLOT_HISTOGRAM_H

#include <vector>
#include <sstream>
#include <string>
#include <datatypes/unit.h>
#include <datatypes/plot.h>

/**
 * This class models a one dimensional histograms with variable number of buckets
 */
class Histogram : public GenericPlot {
    public:
        Histogram(unsigned long number_of_buckets, double min, double max);
        Histogram(unsigned long number_of_buckets, double min, double max, const Unit &unit);
        Histogram(unsigned long number_of_buckets, double min, double max, std::string unit_string);

        explicit Histogram(BinaryReadBuffer &buffer);

        ~Histogram() override;

        static auto compute_unit_string(const Unit &unit) -> std::string;

        void inc(double value);

        void incNoData();

        const std::string toJSON() const override;

        int getCountForBucket(unsigned long bucket) {
            return counts.at(bucket);
        }

        int getNoDataCount() {
            return nodata_count;
        }

        double getMin() {
            return min;
        }

        double getMax() {
            return max;
        }

        unsigned long getNumberOfBuckets() {
            return counts.size();
        }

        /**
         * returns the count of all inserted elements (without NoData)
         */
        int getValidDataCount();

        /**
         * calculates the bucket where a value would be inserted
         */
        int calculateBucketForValue(double value);

        /**
         * calculates the bucket minimum
         */
        double calculateBucketLowerBorder(int bucket);

        /**
         * add a marker
         */
        void addMarker(double bucket, const std::string &label);

        std::unique_ptr<GenericPlot> clone() const override;

        void serialize(BinaryWriteBuffer &buffer, bool is_persistent_memory) const override;

    private:
        std::vector<int> counts;
        int nodata_count = 0;
        double min = 0, max = 0;
        std::string unit;
        std::vector<std::pair<double, std::string>> markers;
};

#endif
