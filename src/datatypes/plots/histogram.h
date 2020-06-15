#ifndef PLOT_HISTOGRAM_H
#define PLOT_HISTOGRAM_H

#include <vector>
#include <sstream>
#include <string>
#include <datatypes/unit.h>
#include <datatypes/plot.h>

/**
 * This class models a one dimensional histogram with a variable number of buckets
 */
class Histogram : public GenericPlot {
    public:
        /**
         * Construct a histogram by specifying the number of buckets and its range.
         */
        Histogram(unsigned long number_of_buckets, double min, double max);

        /**
         * Construct a histogram by specifying the number of buckets and its range.
         * Furthermore, specify a unit for the X axis
         */
        Histogram(unsigned long number_of_buckets, double min, double max, const Unit &unit);

        /**
         * Construct a histogram by specifying the number of buckets and its range.
         * Furthermore, specify the unit string for the X axis
         */
        Histogram(unsigned long number_of_buckets, double min, double max, std::string unit_string);

        /**
         * Deserialize a histogram from a binary buffer
         */
        explicit Histogram(BinaryReadBuffer &buffer);

        ~Histogram() override;

        /**
         * Compute a display X axis unit string from a unit
         */
        static auto compute_unit_string(const Unit &unit) -> std::string;

        /**
         * Increase a histogram bucket by a value in which it fits
         */
        void inc(double value);

        /**
         * Increase the histogram's no data counter
         */
        void incNoData();

        /**
         * Serialize the histogram to a JSON string
         */
        const std::string toJSON() const override;

        /**
         * Retrieve the number of values for one bucket
         */
        int getCountForBucket(unsigned long bucket) {
            return counts.at(bucket);
        }

        /**
         * Retrieve the number of no data values
         */
        int getNoDataCount() {
            return nodata_count;
        }

        /**
         * Retrieve the minimum value of the histogram's range
         */
        double getMin() {
            return min;
        }

        /**
         * Retrieve the maximum value of the histogram's range
         */
        double getMax() {
            return max;
        }

        /**
         * Retrieve the number of buckets
         */
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

        /**
         * Clone the histogram by copying its internals
         */
        std::unique_ptr<GenericPlot> clone() const override;

        /**
         * Serialize the histogram into a binary buffer
         */
        void serialize(BinaryWriteBuffer &buffer, bool is_persistent_memory) const override;

    private:
        std::vector<int> counts;
        int nodata_count = 0;
        double min = 0, max = 0;
        std::string unit;
        std::vector<std::pair<double, std::string>> markers;
};

#endif
