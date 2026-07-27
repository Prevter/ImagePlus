#pragma once
#include <string_view>

enum class ColorSpace {
    BT_601,
    BT_709,
    BT_2020
};

enum class ColorRange {
    Limited,
    Full
};

struct Matrix3x3 {
	float m[3][3];
};

namespace yuv420 {
	constexpr Matrix3x3 BT601_Full  = {{
	    { 1.000f,  0.000f,  1.4020f },
	    { 1.000f, -0.3441f, -0.7141f },
	    { 1.000f,  1.7720f,  0.000f }
	}};

    constexpr Matrix3x3 BT601_Limited = {{
        { 1.164f,  0.000f,  1.596f },
        { 1.164f, -0.392f, -0.813f },
        { 1.164f,  2.017f,  0.000f }
    }};

    constexpr Matrix3x3 BT709_Full  = {{
        { 1.000f,  0.000f,  1.5748f },
        { 1.000f, -0.1873f, -0.4681f },
        { 1.000f,  1.8556f,  0.000f }
    }};

    constexpr Matrix3x3 BT709_Limited = {{
        { 1.164f,  0.000f,  1.793f },
        { 1.164f, -0.213f, -0.534f },
        { 1.164f,  2.115f,  0.000f }
    }};

    constexpr Matrix3x3 BT2020_Full = {{
        { 1.000f,  0.000f,  1.4746f },
        { 1.000f, -0.1646f, -0.5714f },
        { 1.000f,  1.8814f,  0.000f }
    }};

	constexpr Matrix3x3 BT2020_Limited = {{
	    { 1.164f,  0.000f,  1.6787f },
	    { 1.164f, -0.1874f, -0.6505f },
	    { 1.164f,  2.1418f,  0.000f }
	}};

	constexpr Matrix3x3 const& getMatrix(ColorSpace cs, ColorRange cr) {
        switch (cs) {
            case ColorSpace::BT_601: default:
                return cr == ColorRange::Full ? BT601_Full : BT601_Limited;
            case ColorSpace::BT_709:
                return cr == ColorRange::Full ? BT709_Full : BT709_Limited;
            case ColorSpace::BT_2020:
                return cr == ColorRange::Full ? BT2020_Full : BT2020_Limited;
        }
    }
}

inline std::string_view format_as(ColorSpace cs) {
    switch (cs) {
        case ColorSpace::BT_601: return "BT.601";
        case ColorSpace::BT_709: return "BT.709";
        case ColorSpace::BT_2020: return "BT.2020";
        default: return "Unknown";
    }
}

inline std::string_view format_as(ColorRange cr) {
    switch (cr) {
        case ColorRange::Limited: return "Limited";
        case ColorRange::Full: return "Full";
        default: return "Unknown";
    }
}
