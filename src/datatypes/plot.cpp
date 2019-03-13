#include "datatypes/plot.h"
#include "datatypes/plots/histogram.h"


std::unique_ptr<GenericPlot> GenericPlot::deserialize(BinaryReadBuffer &buffer) {
	GenericPlot::Type plotType = buffer.read<GenericPlot::Type>();

	switch (plotType) {
	case GenericPlot::Type::Histogram:
		return std::make_unique<Histogram>(buffer);
	}

	throw MustNotHappenException("Deserialization of Plot failed");
}
