
#include "processing/query.h"


Query::Query(const std::string &operatorgraph, ResultType result, const QueryRectangle &rectangle)
	: operatorgraph(operatorgraph), result(result), rectangle(rectangle) {

}

Query::~Query() {

}
