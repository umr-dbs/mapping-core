#include <gtest/gtest.h>
#include "datatypes/plots/histogram.h"

TEST(Plots, HistogramSerialization) {
	Histogram histogram(10, 0.0, 1.0, "foobar");

	histogram.inc(0.0);
	histogram.inc(0.1337);
	histogram.inc(0.01);
	histogram.inc(0.6);

	histogram.addMarker(1.0, "test");


	auto stream = BinaryStream::makePipe();
	BinaryWriteBuffer wb;
	wb.write(histogram);
	stream.write(wb);

	BinaryReadBuffer rb;
	stream.read(rb);
	std::unique_ptr<GenericPlot> hist = GenericPlot::deserialize(rb);

	EXPECT_EQ(histogram.toJSON(), hist->toJSON());
}
