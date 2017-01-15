#ifndef PLOT_HISTOGRAM_H
#define PLOT_HISTOGRAM_H

#include <vector>
#include <sstream>
#include <string>

#include "datatypes/plot.h"

/**
 * This class models a one dimensional histograms with variable number of buckets
 */
class Histogram : public GenericPlot {
	public:
		Histogram(int number_of_buckets, double min, double max);
		Histogram(BinaryReadBuffer &buffer);

		virtual ~Histogram();

		void inc(double value);
		void incNoData();

		const std::string toJSON() const;

		static const int DEFAULT_NUMBER_OF_BUCKETS = 10000;

		int getCountForBucket(int bucket){
			return counts.at(bucket);
		}
		int getNoDataCount(){
			return nodata_count;
		}
		double getMin(){
			return min;
		}
		double getMax(){
			return max;
		}
		int getNumberOfBuckets(){
			return counts.size();
		}
		/*
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

		std::unique_ptr<GenericPlot> clone() const;

		virtual void serialize(BinaryWriteBuffer &buffer, bool is_persistent_memory) const;

	private:
		std::vector<int> counts;
		int nodata_count;
		double min, max;
		std::vector<std::pair<double, std::string>> markers;
};

#endif
