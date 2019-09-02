#ifndef MAPPING_CORE_GRID_H
#define MAPPING_CORE_GRID_H

#include <unordered_map>
#include <queue>
#include "BoundingBox.h"
#include "CircleClusteringQuadTree.h"

namespace pv {

    /**
     * A Grid that contains non-overlapping circles.
     * It merges circles automatically upon insert if there are multiples circles in a grid cell.
     */
    class Grid {
        public:
            /**
             * Construct a Grid by specifying a bounding box as well a the minimum circle radius and epsilon.
             * @param boundingBox
             * @param r_min
             * @param epsilon
             */
            Grid(const BoundingBox &bounding_box, double x_min, double y_min, const pv::Circle::CommonAttributes &common_attributes);

            /**
             * Inserts a circle into the grid.
             * Merges circles if there are multiple circles in the same grid cell.
             * @param circle
             */
            auto insert(const std::shared_ptr<Circle>& circle) -> void;

            /**
             * Insert all circles of the grid into the CMQ
             * @param tree
             */
            void insert_into(pv::CircleClusteringQuadTree &tree);

        private:
            std::unordered_map<uint16_t, std::shared_ptr<Circle>> cells;
            double offset_x;
            double offset_y;
            double cell_width;
            uint16_t number_of_horitontal_buckets;
            uint16_t number_of_vertical_buckets;
    };

}

#endif //MAPPING_CORE_GRID_H
