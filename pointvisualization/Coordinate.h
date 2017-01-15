#ifndef POINTVISUALIZATION_COORDINATE_H_
#define POINTVISUALIZATION_COORDINATE_H_

#include "Dimension.h"

namespace pv {

	/**
	 * A 2D-coordinate.
	 */
	class Coordinate {
		public:
			/**
			 * Constructs a coordinate given x and y location.
			 * @param x
			 * @param y
			 */
			Coordinate(double x, double y);

            // copy and copy assignment constructor
            Coordinate(const Coordinate& coordinate) = default;
            Coordinate& operator=(const Coordinate&) = default;

			/**
			 * @return x coordinate
			 */
			double getX() const;

			/**
			 * @return y coordinate
			 */
			double getY() const;

			/**
			 * Calculate the euclidean distance.
			 * @param other
			 * @return the distance
			 */
			double euclideanDistance(const Coordinate& other) const;

            /**
             * Calculate the squared euclidean distance for comparison reasons.
             * It does not calculate the square root.
             * @param other
             * @return the squared distance
             */
            double squaredEuclideanDistance(const Coordinate& other) const;

		private:
			double x;
			double y;
	};

}

#endif /* POINTVISUALIZATION_COORDINATE_H_ */
