#include "datatypes/colorizer.h"
#include "datatypes/unit.h"
#include "util/exceptions.h"
#include "util/make_unique.h"

#include <cmath>
#include <vector>
#include <iomanip>

color_t color_from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	return (uint32_t) a << 24 | (uint32_t) b << 16 | (uint32_t) g << 8 | (uint32_t) r;
}

static uint8_t r_from_color(color_t color) {
	return color & 0xff;
}

static uint8_t g_from_color(color_t color) {
	return (color & 0xff00) >> 8;
}

static uint8_t b_from_color(color_t color) {
	return (color & 0xff0000) >> 16;
}

static uint8_t a_from_color(color_t color) {
	return (color & 0xff000000) >> 24;
}

static uint8_t channel_from_double(double c) {
	if (c > 255.0)
		return 255;
	if (c < 0)
		return 0;
	return std::round(c);
}

Colorizer::Colorizer(ColorTable table, Interpolation interpolation) : table(std::move(table)), interpolation(interpolation) {
}

Colorizer::~Colorizer() {
}

void Colorizer::fillPalette(color_t *colors, int num_colors, double min, double max) const {
	double step = (num_colors > 1) ? ((max - min) / (num_colors-1)) : 0;
	for (int c = 0; c < num_colors; c++) {
		double value = min + c*step;
		color_t color;
		if (value <= table[0].value) {
			color = table[0].color;
		}
		else if (value >= table[table.size()-1].value) {
			color = table[table.size()-1].color;
		}
		else {
			color = color_from_rgba(0, 0, 0, 0);
			for (size_t i=1;i<table.size();i++) {
				if (value <= table[i].value) {
					auto last_color = table[i-1].color;
					auto next_color = table[i].color;

					if (interpolation == Interpolation::LINEAR) {
						double fraction = (value-table[i-1].value) / (table[i].value - table[i-1].value);

						uint8_t r = channel_from_double(r_from_color(last_color) * (1-fraction) + r_from_color(next_color) * fraction);
						uint8_t g = channel_from_double(g_from_color(last_color) * (1-fraction) + g_from_color(next_color) * fraction);
						uint8_t b = channel_from_double(b_from_color(last_color) * (1-fraction) + b_from_color(next_color) * fraction);
						uint8_t a = channel_from_double(a_from_color(last_color) * (1-fraction) + a_from_color(next_color) * fraction);

						color = color_from_rgba(r, g, b, a);
					}
					else if (interpolation == Interpolation::NEAREST) {
						if (std::abs(value-table[i-1].value) < std::abs(table[i].value-value))
							color = table[i-1].color;
						else
							color = table[i].color;
					}
					else {
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
	}
	else {
		s << "rgba(" << (int) r_from_color(color) << "," << (int) g_from_color(color) << "," << (int) b_from_color(color) << "," << (a_from_color(color)/255.0) << ")";
	}
}

std::string Colorizer::toJson() const {
	std::ostringstream ss;

	ss << "{ \"interpolation\": \"";
	if (interpolation == Interpolation::LINEAR)
		ss << "linear";
	else if (interpolation == Interpolation::NEAREST)
		ss << "nearest";
	ss << "\", \"breakpoints\": [\n";
	for (size_t i=0;i<table.size();i++) {
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
    if(!json.isMember("breakpoints") || json["breakpoints"].empty()) {
        throw ArgumentException("Missing breakpoints for colorizer");
    }

    Colorizer::ColorTable breakpoints;
    for (auto& breakpoint : json["breakpoints"]) {
        double value = breakpoint["value"].asDouble();
        int r = breakpoint["r"].asInt();
        int g = breakpoint["g"].asInt();
        int b = breakpoint["b"].asInt();
        int a = breakpoint.get("a", 255).asInt();
        breakpoints.emplace_back(value, color_from_rgba(r, g, b, a));
    }

    std::string type = json.get("type", "gradient").asString();

    Interpolation interpolation;
    if (type == "gradient") {
        // TODO: allow nearest neighbor gradient interpolation
        interpolation = Interpolation::LINEAR;
    } else if (type == "palette") {
        // TODO: use discrete
        interpolation = Interpolation::NEAREST;
    } else {
        throw ArgumentException("Unknown type for colorizer");
    }

    return make_unique<Colorizer>(breakpoints, interpolation);
}

std::unique_ptr<Colorizer> Colorizer::greyscale(double min, double max) {
	Colorizer::ColorTable breakpoints {Breakpoint {min, color_from_rgba(0, 0, 0, 255)},
                                       Breakpoint {max, color_from_rgba(0, 0, 0, 255)}};
    return make_unique<Colorizer>(breakpoints, Interpolation::LINEAR);
}

static Colorizer::ColorTable errorBreakpoints {Colorizer::Breakpoint {1, color_from_rgba(255, 0, 0, 255)}};
static Colorizer errorColorizer{errorBreakpoints};
const Colorizer &Colorizer::error() {
	return errorColorizer;
}
