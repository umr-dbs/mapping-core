uint fit_to_interval_0_255(double value, double min, double max, double scale); // forward

/**
 * Computes an RGBA composite out of three input rasters.
 * It transforms each channel into the byte range and composes it into a single unsigned integer.
 * Alpha depends on NO_DATA and fills in the last of the four bytes.
 *
 * Each channel has metadata on their `*_min` and `*_max` values for being able to fit them into the range [0, 255].
 * Moreover, the value `*_scale` ([0, 1]) can be used to accentuate the composite.
 */
__kernel void rgba_composite_kernel(__constant const IN_TYPE0 *red_data, __constant const RasterInfo *red_info,
                                    __constant const IN_TYPE1 *green_data, __constant const RasterInfo *green_info,
                                    __constant const IN_TYPE2 *blue_data, __constant const RasterInfo *blue_info,
                                    __global uint *rgba_data, __constant const RasterInfo *rgba_info,
                                    double red_min, double red_max, double red_scale,
                                    double green_min, double green_max, double green_scale,
                                    double blue_min, double blue_max, double blue_scale) {
    // determine sizes from first raster
    const int row_width = red_info->size[0];
    const int number_of_pixels = red_info->size[0] * red_info->size[1] * red_info->size[2];

    const size_t pixel_index = get_global_id(0) + get_global_id(1) * row_width;
    if (pixel_index >= number_of_pixels) return;

    const bool is_any_nodata = ISNODATA0(red_data[pixel_index], red_info) ||
                               ISNODATA1(green_data[pixel_index], green_info) ||
                               ISNODATA2(blue_data[pixel_index], blue_info);

    uint red = fit_to_interval_0_255((double) red_data[pixel_index], red_min, red_max, red_scale);
    uint green = fit_to_interval_0_255((double) green_data[pixel_index], green_min, green_max, green_scale);
    uint blue = fit_to_interval_0_255((double) blue_data[pixel_index], blue_min, blue_max, blue_scale);
    uint alpha = is_any_nodata ? 0 : 255;

    rgba_data[pixel_index] = (red) | (green << 8) | (blue << 16) | (alpha << 24);
}

/**
 * Fits The incoming value to the interval [0, 255].
 *
 * The parameter `scale` must be in the range [0, 1].
 */
uint fit_to_interval_0_255(double value, double min, double max, double scale) {
    double result = value - min; // shift towards zero
    result /= max - min; // normalize to [0, 1]
    result *= scale; // after scaling, value stays in [0, 1]
    result = clamp(round(255. * result), 0., 255.); // bring value to integer range [0, 255]
    return (uint) result;
}
