
#include "datatypes/raster/raster_priv.h"
#include "datatypes/colorizer.h"

#include <png.h>
#include <png++/png.hpp>
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

    if (overlay && overlay->width == width && overlay->height == height) {
        writeDebugInfoToOverlay(*this, *overlay, colorizer);
    } else if (overlay) {
        // do not use the overlay if the size does not match
        overlay = nullptr;
    }

    switch (colorizer.getInterpolation()) {
        case Colorizer::Interpolation::TREAT_AS_RGBA:
            writeRgbaPng(output, flipx, flipy, overlay);
            break;
        case Colorizer::Interpolation::NEAREST:
        case Colorizer::Interpolation::LINEAR:
        default:
            writePallettedPng(output, colorizer, flipx, flipy, overlay);
            break;
    }
}

template<typename T>
auto Raster2D<T>::writeRgbaPng(std::ostream &output, bool flipx, bool flipy, const Raster2D<uint8_t> *overlay) const -> void {
    if (!std::is_same<T, color_t>()) {
        throw ExporterException("Cannot create RGBA PNG from type different than Raster<uint32_t>");
    }

    png::image<png::rgba_pixel> image(this->width, this->height);
    for (size_t y = 0; y < image.get_height(); ++y) {
        for (size_t x = 0; x < image.get_width(); ++x) {
            color_t value = this->get(
                    flipx ? this->width - x - 1 : x,
                    flipy ? this->height - y - 1 : y
            ); // doing a hard narrowing to `color_t` because of the type check at the top

            auto pixel = png::rgba_pixel(
                    r_from_color(value), g_from_color(value), b_from_color(value), a_from_color(value)
            );

            if (overlay && overlay->get(x, y) > 0) { // override pixel with overlay
                static const png::rgba_pixel OVERLAY_MARKER_COLOR = png::rgba_pixel(255, 20, 147, 255); // deeppink
                pixel = OVERLAY_MARKER_COLOR;
            }

            image.set_pixel(x, y, pixel);
        }
    }

    image.write_stream(output);
}

template<typename T>
auto writeDebugInfoToOverlay(const Raster2D<T> &about, Raster2D<uint8_t> &overlay, const Colorizer &colorizer) -> void {
    const double MARKER_VALUE = 1;
    std::ostringstream text;

    // write scale
    auto previous_precision_value = text.precision(2);
    text << std::fixed << "scale: " << about.pixel_scale_x << ", " << about.pixel_scale_y;
    overlay.print(4, 26, MARKER_VALUE, text.str().c_str());

    text.clear();
    text.precision(previous_precision_value);

    // write unit
    text << "Unit: " << about.dd.unit.getMeasurement() << ", " << about.dd.unit.getUnit();
    overlay.print(4, 36, MARKER_VALUE, text.str().c_str());

    text.clear();

    // write datatype and min/max
    text << GDALGetDataTypeName(about.dd.datatype) << " (" << colorizer.minValue() << " - " << colorizer.maxValue()
         << ")";
    overlay.print(4, 16, MARKER_VALUE, text.str().c_str());

    // mark outer border
    for (uint32_t x = 0; x < overlay.width; ++x) {
        overlay.set(x, 0, MARKER_VALUE);
        overlay.set(x, overlay.height, MARKER_VALUE);
    }
    for (uint32_t y = 0; y < overlay.height; ++y) {
        overlay.set(0, y, MARKER_VALUE);
        overlay.set(overlay.height, y, MARKER_VALUE);
    }
}

template<typename T>
auto Raster2D<T>::writePallettedPng(std::ostream &output, const Colorizer &colorizer, bool flipx, bool flipy,
                                    Raster2D<uint8_t> *overlay) const -> void {

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
        }
        png_write_row(png_ptr, (png_bytep) row);
    }
    delete[] row;

    png_write_end(png_ptr, info_ptr);

    png_destroy_write_struct(&png_ptr, &info_ptr);
}


RASTER_PRIV_INSTANTIATE_ALL
