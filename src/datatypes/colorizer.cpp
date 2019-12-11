#include "datatypes/colorizer.h"
#include "datatypes/unit.h"
#include "util/exceptions.h"

#include <cmath>
#include <vector>
#include <iomanip>

color_t color_from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept {
    return static_cast<uint32_t>(a) << 24u
           | static_cast<uint32_t>(b) << 16u
           | static_cast<uint32_t>(g) << 8u
           | static_cast<uint32_t>(r);
}

color_t color_from_json(const Json::Value &json) {
    int r = json["r"].asInt();
    int g = json["g"].asInt();
    int b = json["b"].asInt();
    int a = json.get("a", 255).asInt();
    return color_from_rgba(r, g, b, a);
}

static uint8_t r_from_color(color_t color) {
    return color & 0x00000000ffu;
}

static uint8_t g_from_color(color_t color) {
    return (color & 0x0000ff00u) >> 8u;
}

static uint8_t b_from_color(color_t color) {
    return (color & 0x00ff0000u) >> 16u;
}

static uint8_t a_from_color(color_t color) {
    return (color & 0xff000000u) >> 24u;
}

static uint8_t channel_from_double(double c) {
    if (c > 255.0)
        return 255;
    if (c < 0)
        return 0;
    return std::round(c);
}

Colorizer::Colorizer(ColorTable table, Interpolation interpolation, color_t nodataColor, color_t defaultColor)
        : table(std::move(table)), interpolation(interpolation), nodataColor(nodataColor), defaultColor(defaultColor) {
}

Colorizer::~Colorizer() = default;

void Colorizer::fillPalette(color_t *colors, int num_colors, double min, double max) const {
    double step = (num_colors > 1) ? ((max - min) / (num_colors - 1)) : 0;
    for (int c = 0; c < num_colors; c++) {
        double value = min + c * step;
        color_t color;
        if (value <= table[0].value) {
            color = table[0].color;
        } else if (value >= table[table.size() - 1].value) {
            color = table[table.size() - 1].color;
        } else {
            color = color_from_rgba(0, 0, 0, 0);
            for (size_t i = 1; i < table.size(); i++) {
                if (value <= table[i].value) {
                    auto last_color = table[i - 1].color;
                    auto next_color = table[i].color;

                    if (interpolation == Interpolation::LINEAR) {
                        double fraction = (value - table[i - 1].value) / (table[i].value - table[i - 1].value);

                        uint8_t r = channel_from_double(
                                r_from_color(last_color) * (1 - fraction) + r_from_color(next_color) * fraction);
                        uint8_t g = channel_from_double(
                                g_from_color(last_color) * (1 - fraction) + g_from_color(next_color) * fraction);
                        uint8_t b = channel_from_double(
                                b_from_color(last_color) * (1 - fraction) + b_from_color(next_color) * fraction);
                        uint8_t a = channel_from_double(
                                a_from_color(last_color) * (1 - fraction) + a_from_color(next_color) * fraction);

                        color = color_from_rgba(r, g, b, a);
                    } else if (interpolation == Interpolation::NEAREST) {
                        if (std::abs(value - table[i - 1].value) < std::abs(table[i].value - value))
                            color = table[i - 1].color;
                        else
                            color = table[i].color;
                    } else if (interpolation == Interpolation::TREAT_AS_RGBA) {
                        throw MustNotHappenException("fillPallette must not be called when using RGBA mode in colorizer");
                    } else {
                        throw MustNotHappenException("Unknown interpolation mode in colorizer");
                    }
                    break;
                }
            }
        }

        colors[c] = color;
    }
}

static void color_as_html(std::ostream &s, color_t color) {
    if (a_from_color(color) == 255) {
        s << "#";
        s << std::setfill('0') << std::hex;
        s << std::setw(2) << (int) r_from_color(color)
          << std::setw(2) << (int) g_from_color(color)
          << std::setw(2) << (int) b_from_color(color);
        s << std::setw(0) << std::dec;
    } else {
        s << "rgba(" << (int) r_from_color(color) << "," << (int) g_from_color(color) << ","
          << (int) b_from_color(color) << "," << (a_from_color(color) / 255.0) << ")";
    }
}

std::string Colorizer::toJson() const {
    std::ostringstream ss;

    ss << R"({ "interpolation": ")";
    switch (interpolation) {
        case Interpolation::LINEAR:
            ss << "linear";
            break;
        case Interpolation::NEAREST:
            ss << "nearest";
            break;
        case Interpolation::TREAT_AS_RGBA:
            ss << "rgba";
            break;
    }
    ss << "\", \"breakpoints\": [\n";
    for (size_t i = 0; i < table.size(); i++) {
        if (i != 0)
            ss << ",\n";
        ss << "[" << table[i].value << ",\"";
        color_as_html(ss, table[i].color);
        ss << "\"]";
    }
    ss << "]}";

    return ss.str();
}

std::unique_ptr<Colorizer> Colorizer::fromJson(const Json::Value &json) {
    if (!json.isMember("breakpoints") || json["breakpoints"].empty()) {
        throw ArgumentException("Missing breakpoints for colorizer");
    }

    Colorizer::ColorTable breakpoints;
    for (auto &breakpoint : json["breakpoints"]) {
        double value = breakpoint["value"].asDouble();
        breakpoints.emplace_back(value, color_from_json(breakpoint));
    }

    color_t nodataColor = defaultNoDataColor;
    if (json.isMember("nodata")) {
        nodataColor = color_from_json(json["nodata"]);
    }

    color_t defaultColor = defaultDefaultColor;
    if (json.isMember("default")) {
        defaultColor = color_from_json(json["default"]);
    }

    std::string type = json.get("type", "gradient").asString();

    Interpolation interpolation;
    if (type == "gradient") {
        // TODO: allow nearest neighbor gradient interpolation
        interpolation = Interpolation::LINEAR;
    } else if (type == "palette") {
        // TODO: use discrete
        interpolation = Interpolation::NEAREST;
    } else if (type == "rgba") {
        interpolation = Interpolation::TREAT_AS_RGBA;
    } else {
        throw ArgumentException("Unknown type for colorizer");
    }

    return std::make_unique<Colorizer>(breakpoints, interpolation, nodataColor, defaultColor);
}

std::unique_ptr<Colorizer> Colorizer::greyscale(double min, double max) {
    Colorizer::ColorTable breakpoints{Breakpoint{min, color_from_rgba(0, 0, 0, 255)},
                                      Breakpoint{max, color_from_rgba(0, 0, 0, 255)}};
    return std::make_unique<Colorizer>(breakpoints, Interpolation::LINEAR);
}

auto Colorizer::rgba() -> std::unique_ptr<Colorizer> {
    const Colorizer::ColorTable value_range{Breakpoint{0x00000000u, color_from_rgba(0, 0, 0, 0)},
                                            Breakpoint{0xffffffffu, color_from_rgba(255, 255, 255, 255)}};
    const auto no_data_color = color_from_rgba(0, 0, 0, 0);
    const auto default_data_color = color_from_rgba(255, 255, 255, 0);
    return std::make_unique<Colorizer>(value_range, Interpolation::TREAT_AS_RGBA, no_data_color, default_data_color);
}

const Colorizer &Colorizer::error() {
    static Colorizer::ColorTable errorBreakpoints{Colorizer::Breakpoint{1, color_from_rgba(255, 0, 0, 255)}};
    static Colorizer errorColorizer{errorBreakpoints};

    return errorColorizer;
}
