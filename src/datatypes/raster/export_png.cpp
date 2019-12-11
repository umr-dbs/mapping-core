
#include "datatypes/raster/raster_priv.h"
#include "datatypes/colorizer.h"

#include <png.h>
#include <sstream>
#include <cmath>


static void png_write_wrapper(png_structp png_ptr, png_bytep data, png_size_t length) {
    auto *stream = static_cast<std::ostream *>(png_get_io_ptr(png_ptr));
    stream->write((const char *) data, length);
}


template<typename T>
void Raster2D<T>::toPNG(std::ostream &output, const Colorizer &colorizer, bool flipx, bool flipy,
                        Raster2D<uint8_t> *overlay) {
    this->setRepresentation(GenericRaster::Representation::CPU);

    if (overlay) {
        // do not use the overlay if the size does not match
        if (overlay->width != width || overlay->height != height)
            overlay = nullptr;
    }

    if (overlay) {
        // Write debug info
        std::ostringstream msg_scale;
        msg_scale.precision(2);
        msg_scale << std::fixed << "scale: " << pixel_scale_x << ", " << pixel_scale_y;
        overlay->print(4, 26, 1, msg_scale.str().c_str());

        std::ostringstream msg_unit;
        msg_unit << "Unit: " << dd.unit.getMeasurement() << ", " << dd.unit.getUnit();
        overlay->print(4, 36, 1, msg_unit.str().c_str());
    }

    // calculate the actual min/max so we can include only the range we require in the palette


    uint32_t colors[256];
    colors[0] = colorizer.getNoDataColor();
    colors[1] = colorizer.getDefaultColor();

    double actual_min = colorizer.minValue();
    double actual_max = colorizer.maxValue();

    unsigned int num_colors = 254;

    if (dd.unit.isDiscrete()) {
        // this assumes classes are consecutive
        num_colors = static_cast<unsigned int>(actual_max - actual_min + 1);
    }

    colorizer.fillPalette(&colors[2], num_colors, actual_min, actual_max);

    if (overlay) {
        std::ostringstream msg;
        msg << GDALGetDataTypeName(dd.datatype) << " (" << actual_min << " - " << actual_max << ")";
        overlay->print(4, 16, 1, msg.str().c_str());
    }


    // prepare PNG output
    png_structp png_ptr;
    png_ptr = png_create_write_struct(
            PNG_LIBPNG_VER_STRING,
            nullptr, nullptr, nullptr
    );
    if (!png_ptr)
        throw ExporterException("Could not initialize libpng");

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, (png_infopp) nullptr);
        throw ExporterException("Could not initialize libpng");
    }

    // write into the provided ostream
    png_set_write_fn(png_ptr, &output, png_write_wrapper, nullptr);

    // start PNG output, headers first
    png_set_IHDR(png_ptr, info_ptr,
                 width, height,
                 8, PNG_COLOR_TYPE_PALETTE,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT
    );

    // transform into png_color array to set PLTE chunk
    png_color colors_rgb[256];
    png_byte colors_a[256];
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = colors[i];
        colors_rgb[i].red = (c >> 0u) & 0xffu;
        colors_rgb[i].green = (c >> 8u) & 0xffu;
        colors_rgb[i].blue = (c >> 16u) & 0xffu;
        colors_a[i] = (c >> 24u) & 0xffu;
    }
    png_set_PLTE(png_ptr, info_ptr, colors_rgb, 256);
    png_set_tRNS(png_ptr, info_ptr, colors_a, 256, nullptr);

    png_write_info(png_ptr, info_ptr);
    png_set_filter(png_ptr, PNG_FILTER_TYPE_BASE, PNG_FILTER_NONE | PNG_FILTER_PAETH);

    auto *row = new uint8_t[width];
    for (uint32_t y = 0; y < height; y++) {
        uint32_t py = flipy ? height - y - 1 : y;
        for (uint32_t x = 0; x < width; x++) {
            uint32_t px = flipx ? width - x - 1 : x;
            T v = get(px, py);
            if (overlay && overlay->get(x, y) == 1) {
                row[x] = 1;
            } else if (dd.is_no_data(v)) {
                row[x] = 0;
            } else if (v < actual_min || v > actual_max) {
                row[x] = 1;
            } else {
                if (actual_min == actual_max)
                    row[x] = 3;
                else {
                    if (dd.unit.isDiscrete())
                        row[x] = v - actual_min + 2;
                    else
                        row[x] = round(253.0 * ((float) v - actual_min) / (actual_max - actual_min)) + 2;
                }
            }
            if (overlay && row[x] != 1) {
                // calculate the distance to the closest image border
                int distx = std::min(x, width - 1 - x);
                int disty = std::min(y, height - 1 - y);

                if ((distx == 0 && (disty < 32 || disty > height / 2 - 16)) ||
                    (disty == 0 && (distx < 32 || distx > width / 2 - 16))) {
                    row[x] = 1;
                }
            }

        }
        png_write_row(png_ptr, (png_bytep) row);
    }
    delete[] row;

    png_write_end(png_ptr, info_ptr);

    png_destroy_write_struct(&png_ptr, &info_ptr);
}


RASTER_PRIV_INSTANTIATE_ALL
