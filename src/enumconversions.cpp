#include "light.hpp"
#include "camera.hpp"

ENUM_CONVERT(bim::Light, Type, Type::NUM_TYPES,
	{ bim::Light::Type::POINT, "point" },
	{ bim::Light::Type::LAMBERT, "lambert" },
	{ bim::Light::Type::DIRECTIONAL, "directional" },
	{ bim::Light::Type::SPOT, "spot" },
	{ bim::Light::Type::SKY, "sky" },
	{ bim::Light::Type::GONIOMETRIC, "goniometric" },
	{ bim::Light::Type::ENVIRONMENT, "environment" }
)

ENUM_CONVERT(bim::Camera, Type, Type::NUM_TYPES,
	{ bim::Camera::Type::PERSPECTIVE, "perspective" },
	{ bim::Camera::Type::ORTHOGRAPHIC, "orthographic" },
	{ bim::Camera::Type::FOCUS, "focus" }
)