#include "Grid.h"

pv::Grid::Grid(const pv::BoundingBox &boundingBox, const double x_min, const double y_min,
               const pv::Circle::CommonAttributes &common_attributes) {
    this->cell_width =
            (2 * common_attributes.getCircleMinRadius() + common_attributes.getEpsilonDistance()) / std::sqrt(2.0);

    double map_width = boundingBox.getHalfDimension().getWidth() * 2;
    double map_height = boundingBox.getHalfDimension().getHeight() * 2;

    this->number_of_horitontal_buckets = static_cast<uint16_t >(std::ceil(map_width / cell_width));
    this->number_of_vertical_buckets = static_cast<uint16_t >(std::ceil(map_height / cell_width));

    this->offset_x = std::floor(x_min / this->cell_width) * this->cell_width;
    this->offset_y = std::floor(y_min / this->cell_width) * this->cell_width;
}

auto pv::Grid::insert(const std::shared_ptr<Circle> &circle) -> void {
    uint32_t grid_x = static_cast<uint32_t >((circle->getCenter().getX() - this->offset_x) / this->cell_width);
    uint32_t grid_y = static_cast<uint32_t >((circle->getCenter().getY() - this->offset_y) / this->cell_width);

    // TODO: use Z-order instead of xy-order
    uint32_t grid_pos = grid_y * number_of_horitontal_buckets + grid_x;

    auto iterator = this->cells.find(grid_pos);
    if (iterator != this->cells.end()) {
        // merge
        Circle merged_circle = iterator->second->merge(*circle);
        iterator->second = std::make_shared<Circle>(merged_circle);
    } else {
        // insert
        this->cells.insert(iterator, {grid_pos, circle});
    }
}

void pv::Grid::insert_into(pv::CircleClusteringQuadTree &tree) {
    std::priority_queue<std::pair<uint32_t, std::shared_ptr<Circle>>> bucket_circles;

    for (size_t bucket_i = 0; bucket_i < this->cells.bucket_count(); ++bucket_i) {
        for (auto bucket_iter = this->cells.begin(bucket_i); bucket_iter != this->cells.end(bucket_i); ++bucket_iter) {
            bucket_circles.emplace(*bucket_iter);
        }

        while (!bucket_circles.empty()) {
            tree.insert(bucket_circles.top().second);
            bucket_circles.pop();
        }
    }
}
