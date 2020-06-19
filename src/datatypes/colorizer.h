#ifndef DATATYPES_COLORIZER_H
#define DATATYPES_COLORIZER_H

#include <memory>
#include <vector>

#include <cstdint>
#include <json/json.h>


using color_t = uint32_t;

/// Create a color from rgba bytes
auto color_from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) noexcept -> color_t;

/// Extracts the red portion (byte) of a color
auto r_from_color(color_t color) -> uint8_t;

/// Extracts the green portion (byte) of a color
auto g_from_color(color_t color) -> uint8_t;

/// Extracts the blue portion (byte) of a color
auto b_from_color(color_t color) -> uint8_t;

/// Extracts the alpha portion (byte) of a color
auto a_from_color(color_t color) -> uint8_t;

class Unit;

static color_t defaultNoDataColor = color_from_rgba(0, 0, 0, 0);
static color_t defaultDefaultColor = color_from_rgba(255, 0, 255, 0);

/**
 * This is a basic Colorizer, based on a table of value:color pairs.
 * The color of a pixel is determined by interpolating between the nearest Breakpoints.
 */
class Colorizer {
    public:
        /**
         * A breakpoint that maps a value to a color
         */
        struct Breakpoint {
            Breakpoint(double v, color_t c) : value(v), color(c) {}

            double value;
            color_t color;
        };

        /**
         * An interpolation method between break points
         */
        enum class Interpolation {
                NEAREST,
                LINEAR,
                LOGARITHMIC,
                TREAT_AS_RGBA,
        };
        using ColorTable = std::vector<Breakpoint>;

        /**
         * Create a new colorizer by specifying a color breakpoint table, an interpolation method, a no data color and a default color
         */
        explicit Colorizer(ColorTable table, Interpolation interpolation = Interpolation::LINEAR,
                           color_t nodataColor = defaultNoDataColor, color_t defaultColor = defaultDefaultColor);

        virtual ~Colorizer();

        /**
         * Fill a palette of `num_colors` colors by using the colorizer's breakpoints
         */
        void fillPalette(color_t *colors, unsigned int num_colors, double min, double max) const;

        /**
         * Serialize this colorizer to a JSON string
         */
        std::string toJson() const;

        /**
         * Retrieve the interpolation enum variant
         */
        auto getInterpolation() const -> Interpolation { return this->interpolation; }

        /**
         * create colorizer from json specification
         */
        static std::unique_ptr<Colorizer> fromJson(const Json::Value &json);

        /**
         * create greyscale colorizer based on min/max values
         */
        static std::unique_ptr<Colorizer> greyscale(double min, double max);

        /**
         * Create an RGBA colorizer.
         */
        static auto rgba() -> std::unique_ptr<Colorizer>;

        /**
         * get the default colorizer for error output
         */
        static const Colorizer &error();

        /**
         * Retrieve the minimum value of the color breakpoint table
         */
        double minValue() const {
            return table.front().value;
        }

        /**
         * Retrieve the maximum value of the color breakpoint table
         */
        double maxValue() const {
            return table.back().value;
        }

        /**
         * Retrieve no data color
         */
        color_t getNoDataColor() const {
            return nodataColor;
        }

        /**
         * Retrieve the default color
         */
        color_t getDefaultColor() const {
            return defaultColor;
        }

    private:
        ColorTable table;
        Interpolation interpolation;
        color_t nodataColor = defaultNoDataColor;
        color_t defaultColor = defaultDefaultColor;
};

#endif
