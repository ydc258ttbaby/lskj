#pragma once

enum Enum_property {
	Enum_diameter,
	Enum_area,
	Enum_perimeter,
	Enum_volume,
	Enum_eccentricity
};

struct Condition
{
	Enum_property m_property;
	float m_min;
	float m_max;
	int m_b_and;
};
