#ifndef PROCESSING_QUERY_H
#define PROCESSING_QUERY_H

#include "operators/operator.h"
#include "operators/queryrectangle.h"

class Query {
	public:
		enum class ResultType {
			RASTER = 0,
			POINTS = 1,
			LINES = 2,
			POLYGONS = 3,
			RASTER_TIME_SERIES = 4,
			PLOT = 5,
			ERROR = 6
		};
		Query(const std::string &operatorgraph, ResultType result, const QueryRectangle &rectangle);
		~Query();

		std::string operatorgraph;
		ResultType result;
		QueryRectangle rectangle;
};

#endif
