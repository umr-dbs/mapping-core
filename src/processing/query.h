#ifndef PROCESSING_QUERY_H
#define PROCESSING_QUERY_H

#include "operators/operator.h"
#include "operators/queryrectangle.h"

class Query {
	public:
		enum class ResultType {
			RASTER,
			POINTS,
			LINES,
			POLYGONS,
			PLOT,
			ERROR
		};
		Query(const std::string &operatorgraph, ResultType result, const QueryRectangle &rectangle);
		~Query();

		std::string operatorgraph;
		ResultType result;
		QueryRectangle rectangle;
};

#endif
