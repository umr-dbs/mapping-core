#ifndef PLOT_TEXT_H
#define PLOT_TEXT_H

#include <string>
#include <json/json.h>

#include "datatypes/plot.h"

/**
 * This plot outputs json
 */
class JsonPlot : public GenericPlot {
	public:
		JsonPlot(Json::Value json);
		virtual ~JsonPlot();

		const std::string toJSON() const;

		std::unique_ptr<GenericPlot> clone() const {
			return std::make_unique<JsonPlot>(json);
		}

	private:
		Json::Value json;
};

#endif
