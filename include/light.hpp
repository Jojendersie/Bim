#pragma once

#include <ei/vector.hpp>
#include <string>

namespace bim {

	// Base class to simplify interfaces with arbitrary light types.
	struct Light
	{
		enum class Type {
			POINT,
			LAMBERT,
			DIRECTIONAL,
			SPOT,
			SKY,
			GONIOMETRIC,
			ENVIRONMENT
		};

		Light(Type _type) : type(_type) {}
		virtual ~Light() = default;

		const Type type;
		std::string name;
	};

	struct PointLight : public Light
	{
		ei::Vec3 position;
		ei::Vec3 intensity;		// [cd = lm / sr]
	};

	struct LambertLight : public Light
	{
		ei::Vec3 position;
		ei::Vec3 normal;
		ei::Vec3 intensity;		// [cd = lm / sr]
	};

	struct DirectionalLight : public Light
	{
		ei::Vec3 direction;
		ei::Vec3 irradiance;	// [lm / m^2]
	};

	// A spot light with the intensity distribution:
	// I(t) = I0 * ((t - 1 + cos(halfAngle)) / cos(halfAngle))^falloff
	// with t is dot(spot.direction, query.direction)
	struct SpotLight : public Light
	{
		ei::Vec3 position;
		ei::Vec3 direction;
		ei::Vec3 peakIntensity;	// [cd = lm / sr]
		float falloff;
		float halfAngle;
	};

	// Preetham skylight model with a few parameters
	struct SkyLight : public Light
	{
		ei::Vec3 sunDirection;
		float turbidity;
		bool aerialPerspective;
	};

	struct GoniometricLight : public Light
	{
		ei::Vec3 position;
		ei::Vec3 intensityScale;
		std::string intensityMap;	// [cd = lm / sr]
	};

	struct EnvironmentLight : public Light
	{
		std::string radianceMap;	// [cd / m^2]
	};
}
