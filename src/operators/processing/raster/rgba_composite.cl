__kernel void rgba_composite_kernel(__constant const IN_TYPE0 *red_data, __constant const RasterInfo *red_info,
                                    __constant const IN_TYPE1 *green_data, __constant const RasterInfo *green_info,
                                    __constant const IN_TYPE2 *blue_data, __constant const RasterInfo *blue_info,
                                    __global uint *rgba_data, __constant const RasterInfo *rgba_info,
                                    double red_min, double red_max, double red_factor,
                                    double green_min, double green_max, double green_factor,
                                    double blue_min, double blue_max, double blue_factor) {
    // determine sizes from first raster
    const int row_width = red_info->size[0];
    const int number_of_pixels = red_info->size[0] * red_info->size[1] * red_info->size[2];

    const size_t pixel_index = get_global_id(0) + get_global_id(1) * row_width;
    if (pixel_index >= number_of_pixels) return;

    const bool is_any_nodata = ISNODATA0(red_data[pixel_index], red_info) ||
                               ISNODATA1(green_data[pixel_index], green_info) ||
                               ISNODATA2(blue_data[pixel_index], blue_info);

    // initialize `rgba_data` with alpha value, depending on if there is any NODATA
    rgba_data[pixel_index] = is_any_nodata ? 0 : (255 << 24);

    double red = (double) red_data[pixel_index];
    red -= red_min; // shift to zero
    red /= red_max - red_min; // normalize to [0, 1]
    red *= red_factor;
    red = clamp(round(255. * red), 0., 255.); // bring value to integer range [0, 255]

    rgba_data[pixel_index] |= (uint) red;

    double green = (double) green_data[pixel_index];
    green -= green_min; // shift to zero
    green /= green_max - green_min; // normalize to [0, 1]
    green *= green_factor;
    green = clamp(round(255. * green), 0., 255.); // bring value to integer range [0, 255]

    rgba_data[pixel_index] |= ((uint) green) << 8;

    double blue = (double) blue_data[pixel_index];
    blue -= blue_min; // shift to zero
    blue /= blue_max - blue_min; // normalize to [0, 1]
    blue *= blue_factor;
    blue = clamp(round(255. * blue), 0., 255.); // bring value to integer range [0, 255]

    rgba_data[pixel_index] |= ((uint) blue) << 16;
}
