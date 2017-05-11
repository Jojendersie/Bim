#pragma once

#include <ei/vector.hpp>
#include <string>
#include "../deps/EnumConverter.h"

namespace bim {

	// Base class to simplify interfaces with arbitrary light types.
	class Light
	{
	public:
		enum class Type {
			POINT,
			LAMBERT,
			DIRECTIONAL,
			SPOT,
			SKY,
			GONIOMETRIC,
			ENVIRONMENT,

			NUM_TYPES
		};
		ENUM_CONVERT_FUNC(Type)

		explicit Light(Type _type, std::string _name = "") :
			type(_type)
		{
			static int s_genericLightName = 0;
			if(!_name.empty()) name = move(_name);
			else name = s_genericLightName++;
		}
		virtual ~Light() = default;

		const Type type;
		std::string name;
	};

	class PointLight : public Light
	{
	public:
		explicit PointLight(std::string _name = "") :
			Light(Type::POINT, move(_name))
		{}

		PointLight(const ei::Vec3& _position, const ei::Vec3& _intensity, std::string _name = "") :
			Light(Type::POINT, move(_name)),
			position(_position),
			intensity(_intensity)
		{}

		ei::Vec3 position;
		ei::Vec3 intensity;		// [cd = lm / sr]
	};

	class LambertLight : public Light
	{
	public:
		explicit LambertLight(std::string _name = "") :
			Light(Type::LAMBERT, move(_name))
		{}

		LambertLight(const ei::Vec3& _position, const ei::Vec3& _normal, const ei::Vec3& _intensity, std::string _name = "") :
			Light(Type::LAMBERT, move(_name)),
			position(_position),
			normal(_normal),
			intensity(_intensity)
		{}

		ei::Vec3 position;
		ei::Vec3 normal;
		ei::Vec3 intensity;		// [cd = lm / sr]
	};

	class DirectionalLight : public Light
	{
	public:
		explicit DirectionalLight(std::string _name = "") :
			Light(Type::DIRECTIONAL, move(_name))
		{}

		DirectionalLight(const ei::Vec3& _direction, const ei::Vec3& _irradiance, std::string _name = "") :
			Light(Type::DIRECTIONAL, move(_name)),
			direction(_direction),
			irradiance(_irradiance)
		{}

		ei::Vec3 direction;
		ei::Vec3 irradiance;	// [lm / m^2]
	};

	// A spot light with the intensity distribution:
	// I(t) = I0 * ((t - 1 + cos(halfAngle)) / cos(halfAngle))^falloff
	// with t is dot(spot.direction, query.direction)
	class SpotLight : public Light
	{
	public:
		explicit SpotLight(std::string _name = "") :
			Light(Type::SPOT, move(_name))
		{}

		SpotLight(const ei::Vec3& _position, const ei::Vec3& _direction, const ei::Vec3& _intensity, float _falloff, float _halfAngle, std::string _name = "") :
			Light(Type::SPOT, move(_name)),
			position(_position),
			direction(_direction),
			peakIntensity(_intensity),
			falloff(_falloff),
			halfAngle(_halfAngle)
		{}

		ei::Vec3 position;
		ei::Vec3 direction;
		ei::Vec3 peakIntensity;	// [cd = lm / sr]
		float falloff;
		float halfAngle;
	};

	// Preetham skylight model with a few parameters
	class SkyLight : public Light
	{
	public:
		explicit SkyLight(std::string _name = "") :
			Light(Type::SPOT, move(_name))
		{}

		SkyLight(const ei::Vec3& _sunDirection, float _turbidity, bool _aerialPerspective, std::string _name = "") :
			Light(Type::SPOT, move(_name)),
			sunDirection(_sunDirection),
			turbidity(_turbidity),
			aerialPerspective(_aerialPerspective)
		{}

		ei::Vec3 sunDirection;
		float turbidity;
		bool aerialPerspective;
	};

	class GoniometricLight : public Light
	{
	public:
		explicit GoniometricLight(std::string _name = "") :
			Light(Type::GONIOMETRIC, move(_name))
		{}

		GoniometricLight(const ei::Vec3& _position, const ei::Vec3& _intensityScale, const std::string& _intensityMap, std::string _name = "") :
			Light(Type::GONIOMETRIC, _name),
			position(_position),
			intensityScale(_intensityScale),
			intensityMap(_intensityMap)
		{}

		ei::Vec3 position;
		ei::Vec3 intensityScale;
		std::string intensityMap;	// [cd = lm / sr]
	};

	class EnvironmentLight : public Light
	{
	public:
		explicit EnvironmentLight(std::string _name = "") :
			Light(Type::ENVIRONMENT, move(_name))
		{}

		explicit EnvironmentLight(const std::string& _radianceMap, std::string _name = "") :
			Light(Type::ENVIRONMENT, move(_name)),
			radianceMap(_radianceMap)
		{}

		std::string radianceMap;	// [cd / m^2]
	};
}
