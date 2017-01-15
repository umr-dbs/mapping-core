#include <limits>
#include "Circle.h"

using namespace pv;

Circle::NumericAttribute::NumericAttribute(double initialValue)
        : average(initialValue), averageOfSquared(pow(initialValue, 2)){}

Circle::NumericAttribute Circle::NumericAttribute::merge(
        const Circle::NumericAttribute &other,
        unsigned int thisWeight,
        unsigned int otherWeight
) const {
    NumericAttribute newAttribute{
        ((this->average * thisWeight) + (other.average * otherWeight)) / (thisWeight+otherWeight)
    };
    newAttribute.averageOfSquared =
        ((this->averageOfSquared * thisWeight) + (other.averageOfSquared * otherWeight)) / (thisWeight+otherWeight);
    return newAttribute;
}

double Circle::NumericAttribute::getAverage() const {
    return average;
}

double Circle::NumericAttribute::getVariance() const {
    return averageOfSquared - pow(average, 2);
}

std::string Circle::TextDictionary::getTextForKey(std::size_t key) const {
    return keyResolutionMap[key];
}

std::size_t Circle::TextDictionary::getKeyForText(std::string& text) {
    if (textResolutionMap.count(text) > 0) {
        return textResolutionMap[text];
    } else {
        // create new entry in dictionary
        keyResolutionMap.push_back(text);
        std::size_t textKey = keyResolutionMap.size() - 1;
        textResolutionMap[text] = textKey;

        return textKey;
    }
}

Circle::TextAttribute::TextAttribute(std::string initialValue, Coordinate coordinate, CommonAttributes& commonAttributes)
        : textDictionary(commonAttributes.getTextDictionary()) {
    textKeys.reserve(maximumTextArrayLength);
    coordinates.reserve(maximumTextArrayLength);

    // insert initialValue at first position
    textKeys.push_back(textDictionary.getKeyForText(initialValue));
    coordinates.push_back(coordinate);
}

Circle::TextAttribute::TextAttribute(TextDictionary& textDictionary)
        : textDictionary(textDictionary) {
    textKeys.reserve(maximumTextArrayLength);
}

Circle::TextAttribute Circle::TextAttribute::merge(
    const Circle::TextAttribute &other,
    const Coordinate& center
) const {
    TextAttribute newAttribute{textDictionary};
    std::vector<double> squaredDistances;
    double largestSquaredDistanceIndex = 0;

    // copy texts from this
    newAttribute.textKeys.insert(newAttribute.textKeys.begin(), this->textKeys.begin(), this->textKeys.end());
    newAttribute.coordinates.insert(
        newAttribute.coordinates.begin(), this->coordinates.begin(), this->coordinates.end()
    );
    for (const Coordinate& coordinate : newAttribute.coordinates) {
        // calculate and store squared distances
        double squaredDistance = coordinate.squaredEuclideanDistance(center);
        squaredDistances.push_back(squaredDistance);

        if (squaredDistance > squaredDistances[largestSquaredDistanceIndex]) {
            largestSquaredDistanceIndex = squaredDistances.size() - 1;
        }
    }

    // loop through other list, look for duplicates or swaps
    for (std::size_t otherIndex = 0; otherIndex < other.textKeys.size(); ++otherIndex) {
        // duplicate?
        bool duplicate = false;
        for (std::size_t newAttributeIndex = 0; newAttributeIndex < newAttribute.textKeys.size(); ++newAttributeIndex) {
            if (other.textKeys[otherIndex] == newAttribute.textKeys[newAttributeIndex]) {
                double otherSquaredDistance = other.coordinates[otherIndex].squaredEuclideanDistance(center);

                // swap coordinate if other coordinate's distance is smaller
                if (otherSquaredDistance < squaredDistances[newAttributeIndex]) {
                    newAttribute.coordinates[newAttributeIndex] = other.coordinates[otherIndex];
                    squaredDistances[newAttributeIndex] = otherSquaredDistance;
                }

                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        // if not a duplicate -> decide wether to insert it or not
        if (newAttribute.textKeys.size() < maximumTextArrayLength) {
            // iff there is space, just insert it
            newAttribute.textKeys.push_back(other.textKeys[otherIndex]);
            newAttribute.coordinates.push_back(other.coordinates[otherIndex]);
            squaredDistances.push_back(other.coordinates[otherIndex].squaredEuclideanDistance(center));
        } else {
            // if there is no space left, check if the item's distance is smaller than any existing item's distance.
            double squaredDistance = other.coordinates[otherIndex].squaredEuclideanDistance(center);

            if (squaredDistance < squaredDistances[largestSquaredDistanceIndex]) {
                // swap
                newAttribute.textKeys[largestSquaredDistanceIndex] = other.textKeys[otherIndex];
                newAttribute.coordinates[largestSquaredDistanceIndex] = other.coordinates[otherIndex];
                squaredDistances[largestSquaredDistanceIndex] = squaredDistance;

                // update largest squared distance index
                for (std::size_t newAttributeIndex = 0; newAttributeIndex < newAttribute.textKeys.size(); ++newAttributeIndex) {
                    if (squaredDistances[newAttributeIndex] > squaredDistances[largestSquaredDistanceIndex]) {
                        largestSquaredDistanceIndex = newAttributeIndex;
                    }
                }
            }

        }
    }

    return newAttribute;
}

std::vector<std::string> Circle::TextAttribute::getTexts() const {
    std::vector<std::string> texts;
    for (auto textKey : textKeys) {
        texts.push_back(textDictionary.getTextForKey(textKey));
    }
    return texts;
};

std::size_t Circle::TextAttribute::maximumTextArrayLength = 5;

Circle::CommonAttributes::CommonAttributes(double circleMinRadius, double epsilonDistance) :
        CIRCLE_MIN_RADIUS(circleMinRadius), EPSILON_DISTANCE(epsilonDistance) {}

double Circle::CommonAttributes::getCircleMinRadius() const {
    return CIRCLE_MIN_RADIUS;
}

double Circle::CommonAttributes::getEpsilonDistance() const {
    return EPSILON_DISTANCE;
}

Circle::TextDictionary &Circle::CommonAttributes::getTextDictionary() {
    return textDictionary;
};

Circle::Circle(Coordinate center, CommonAttributes& commonAttributes)
    : center(center), commonAttributes(commonAttributes), numberOfPoints(1) {
	radius = calculateRadius(numberOfPoints);
}

Circle::Circle(
    Coordinate center,
    CommonAttributes& commonAttributes,
    std::map<std::string, TextAttribute> textAttributes,
    std::map<std::string, NumericAttribute> numericAttributes
) : center(center),
    commonAttributes(commonAttributes),
    numberOfPoints(1),
    numericAttributes(numericAttributes),
    textAttributes(textAttributes) {
    radius = calculateRadius(numberOfPoints);
};

Circle::Circle(Coordinate center, CommonAttributes& commonAttributes, unsigned int numberOfPoints)
				: center(center), commonAttributes(commonAttributes), numberOfPoints(numberOfPoints) {
	radius = calculateRadius(numberOfPoints);
}

Circle Circle::merge(Circle& other) const {
	unsigned int newWeight = numberOfPoints + other.numberOfPoints;
	Coordinate newCenter = Coordinate((center.getX() * numberOfPoints + other.center.getX() * other.numberOfPoints) / newWeight,
									  (center.getY() * numberOfPoints + other.center.getY() * other.numberOfPoints) / newWeight);
    Circle newCircle = Circle(newCenter, other.commonAttributes, newWeight);

    // add numeric attributes
    for(auto iterator = this->numericAttributes.begin(); iterator != this->numericAttributes.end(); ++iterator) {
        const std::string& key = iterator->first;
        const NumericAttribute& thisValue = iterator->second;
        const NumericAttribute& otherValue = other.numericAttributes.at(key);

        newCircle.numericAttributes.erase(key);
        newCircle.numericAttributes.insert(
            std::make_pair(
                key,
                std::move(thisValue.merge(otherValue, this->numberOfPoints, other.numberOfPoints))
            )
        );

        //newCircle.numericAttributes[key] = thisValue.merge(otherValue, this->numberOfPoints, other.numberOfPoints);
    }

    // add text attributes
    for(auto iterator = this->textAttributes.begin(); iterator != this->textAttributes.end(); ++iterator) {
        const std::string& key = iterator->first;
        const TextAttribute& thisValue = iterator->second;
        const TextAttribute& otherValue = other.textAttributes.at(key);

        newCircle.textAttributes.erase(key);
        newCircle.textAttributes.insert(
            std::make_pair(
                key,
                std::move(thisValue.merge(otherValue, center))
            )
        );

        // newCircle.textAttributes[key] = thisValue.merge(otherValue, center);
    }

	return newCircle;
}

Coordinate Circle::getCenter() const {
	return this->center;
}
double Circle::getX() const {
	return this->center.getX();
}
double Circle::getY() const {
	return this->center.getY();
}
double Circle::getRadius() const {
	return this->radius;
}

int Circle::getNumberOfPoints() const {
	return numberOfPoints;
}

bool Circle::intersects(Circle& circle) const {
	return sqrt(pow(center.getX() - circle.getCenter().getX(), 2) + pow(center.getY() - circle.getCenter().getY(), 2)) < (radius + circle.getRadius() + commonAttributes.getEpsilonDistance());
}

double Circle::calculateRadius(int numberOfPoints) const {
	return commonAttributes.getCircleMinRadius() + log(numberOfPoints);
}

const std::map<std::string, Circle::NumericAttribute> Circle::getNumericAttributes() const {
    return numericAttributes;
};

const std::map<std::string, Circle::TextAttribute> Circle::getTextAttributes() const {
    return textAttributes;
};

std::string Circle::to_string() const {
	std::stringstream toString;
	toString << "Center <" << center.getX() << ", " << center.getY() << ">, radius=" << radius << "]";
	return toString.str();
}
