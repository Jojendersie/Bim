#include "light.hpp"

ENUM_CONVERT(bim::Light, Type, Type::NUM,
	{ bim::Light::Type::POINT, "point" },
	{ bim::Light::Type::LAMBERT, "lambert" },
	{ bim::Light::Type::DIRECTIONAL, "directional" },
	{ bim::Light::Type::SPOT, "spot" },
	{ bim::Light::Type::SKY, "sky" },
	{ bim::Light::Type::GONIOMETRIC, "goniometric" },
	{ bim::Light::Type::ENVIRONMENT, "environment" }
)