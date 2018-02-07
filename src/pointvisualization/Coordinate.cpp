#include <cmath>
#include "Coordinate.h"

using namespace pv;

Coordinate::Coordinate(double x, double y) : x(x), y(y) {}

double Coordinate::getX() const {
	return this->x;
}

double Coordinate::getY() const {
	return this->y;
}

double Coordinate::euclideanDistance(const Coordinate &other) const {
    return sqrt(squaredEuclideanDistance(other));
}

double Coordinate::squaredEuclideanDistance(const Coordinate &other) const {
	return pow(this->x - other.x, 2) + pow(this->y - other.y, 2);
}

