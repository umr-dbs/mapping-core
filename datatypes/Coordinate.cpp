#include "Coordinate.h"

Coordinate::Coordinate(BinaryReadBuffer &buffer) {
    buffer.read(&x);
    buffer.read(&y);
}
void Coordinate::serialize(BinaryWriteBuffer &buffer, bool) const {
    buffer << x << y;
}
