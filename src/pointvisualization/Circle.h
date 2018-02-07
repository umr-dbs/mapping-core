#ifndef POINTVISUALIZATION_CIRCLE_H_
#define POINTVISUALIZATION_CIRCLE_H_

#include <vector>
#include <memory>
#include <cmath>
#include <string>
#include <sstream>
#include <map>
#include <numeric>
#include <algorithm>

#include "Coordinate.h"

namespace pv {

	/**
	 * A circle cluster consisting of a center, a radius and a number of points.
	 * The radius calculates of the minimum radius and the number of points (cluster).
	 */
	class Circle {
		public:
            /**
             * This class represents a numeric attribute value aggregation.
             */
			class NumericAttribute {
				public:
					/**
					 * Construct a NumericAttribute using the initialValue for the average of cardinality one.
					 * @param initialValue
					 */
					NumericAttribute(double initialValue);

                    /**
                     * Merge this attribute with another one, specifying weights
                     * @param other the other attribute
                     * @return a new numeric attribute
                     */
                    NumericAttribute merge(
                            const NumericAttribute& other,
                            unsigned int thisWeight,
                            unsigned int otherWeight
                    ) const;

					/**
					 * Return the average of the numeric attribute regarding the circle assignement.
					 * @return the average
					 */
					double getAverage() const;

					/**
					 * Return the variance of the numeric attribute regarding the circle assignment.
					 * @return the variance
					 */
					double getVariance() const;
				private:
					double average;
					double averageOfSquared;
			};

            /**
             * This dictionary allows efficient storage of strings and maps them to numbers.
             */
            class TextDictionary {
                public:
                    std::string getTextForKey(std::size_t key) const;
                    std::size_t getKeyForText(std::string& text);
                private:
                    std::map<std::string, std::size_t> textResolutionMap;
                    std::vector<std::string> keyResolutionMap;
            };

            /**
             * Attributes that are shared among each circle
             */
            class CommonAttributes {
            public:
                /**
                 * Create common attributes
                 * @param circleMinRadius
                 * @param epsilonDistance
                 * @return a common attributes instance
                 */
                CommonAttributes(double circleMinRadius, double epsilonDistance);

                /**
                 * @return the minimum radius
                 */
                double getCircleMinRadius() const;

                /**
                 * @return the epsilon distance
                 */
                double getEpsilonDistance() const;

                /**
                 * @return a reference to the text dictionary
                 */
                Circle::TextDictionary& getTextDictionary();
            private:
                double CIRCLE_MIN_RADIUS;
                double EPSILON_DISTANCE;
                Circle::TextDictionary textDictionary;
            };

            /**
             * This class represents a textual attribute value aggregation.
             */
			class TextAttribute {
				public:
					/**
					 * Construct a TextAttribute using an initial value.
					 * @param initialValue
					 * @param textResolutionMap
					 */
					TextAttribute(
						std::string initialValue,
                        Coordinate coordinate,
                        Circle::CommonAttributes& commonAttributes
					);

                    /**
                     * Merge this attribute with another one, specifying weights
                     * @param other the other attribute
                     * @return a new text attribute
                     */
                    TextAttribute merge(
                        const TextAttribute& other,
                        const Coordinate& center
                    ) const;

                    /**
                     * Get the text values of this attribute.
                     * @return a list of text values
                     */
					std::vector<std::string> getTexts() const;
				private:
                    /**
                     * This constructor is used for merge operations.
                     * @param textDictionary
                     * @return an empty text attribute
                     */
                    TextAttribute(
                        Circle::TextDictionary& textDictionary
                    );

                    Circle::TextDictionary& textDictionary;

					std::vector<std::size_t> textKeys;
                    std::vector<Coordinate> coordinates;

                    static std::size_t maximumTextArrayLength;
			};

			/**
			 * Constructs a circle given a coordinate center, a minimum radius and an epsilon distance.
			 * @param circleMinRadius Minimum radius independent of the number of points.
			 * @param epsilonDistance Minimum distance between two points.
			 */
			Circle(Coordinate center, CommonAttributes& commonAttributes);

            /**
             * Constructs a circle given a coordinate center, a minimum radius and an epsilon distance.
             * @param center
             * @param commonAttributes
             * @param textAttributes
             * @param numericAttributes
             */
            Circle(
                Coordinate center,
                CommonAttributes& commonAttributes,
                std::map<std::string, TextAttribute> textAttributes,
                std::map<std::string, NumericAttribute> numericAttributes
            );

			/**
			 * Merge this circle with another one and construct a new one.
			 * @param other
			 * @return merged circle
			 */
			Circle merge(Circle& other) const;

			/**
			 * @return center
			 */
			Coordinate getCenter() const;
			/**
			 * @return x
			 */
			double getX() const;
			/**
			 * @return y
			 */
			double getY() const;
			/**
			 * @return radius
			 */
			double getRadius() const;
			/**
			 * @return number of points
			 */
			int getNumberOfPoints() const;

			/**
			 * Checks for intersection of this circle with another one.
			 * (Takes epsilonDistance into account.)
			 * @return true if circle intersect, false else.
			 */
			bool intersects(Circle& circle) const;

            const std::map<std::string, NumericAttribute> getNumericAttributes() const;
            const std::map<std::string, TextAttribute> getTextAttributes() const;

			/**
			 * String representation of circle.
			 */
			std::string to_string() const;

		private:
            /**
             * Constructs a circle given a coordinate center, a minimum radius and an epsilon distance.
             * @param circleMinRadius Minimum radius independent of the number of points.
             * @param epsilonDistance Minimum distance between two points.
             * @param numberOfPoints Number of points in the cluster.
             */
            Circle(Coordinate center, CommonAttributes& commonAttributes, unsigned int numberOfPoints);

			Coordinate center;
			double radius;

            CommonAttributes& commonAttributes;

			unsigned int numberOfPoints;

			double calculateRadius(int numberOfPoints) const;

            std::map<std::string, NumericAttribute> numericAttributes;
            std::map<std::string, TextAttribute> textAttributes;
	};

}

#endif /* POINTVISUALIZATION_CIRCLE_H_ */
