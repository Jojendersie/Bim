#pragma once

#include <ei/vector.hpp>
#include <string>
#include "../deps/EnumConverter.h"

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
			ENVIRONMENT,

			NUM_TYPES
		};
		ENUM_CONVERT_FUNC(Type)

		explicit Light(Type _type, const char* _name = nullptr) :
			type(_type)
		{
			static int s_genericLightName = 0;
			if(_name) name = _name;
			else name = s_genericLightName++;
		}
		virtual ~Light() = default;

		const Type type;
		std::string name;
	};

	struct PointLight : public Light
	{
		explicit PointLight(const char* _name = nullptr) :
			Light(Type::POINT, _name)
		{}

		PointLight(const ei::Vec3& _position, const ei::Vec3& _intensity, const char* _name = nullptr) :
			Light(Type::POINT, _name),
			position(_position),
			intensity(_intensity)
		{}

		ei::Vec3 position;
		ei::Vec3 intensity;		// [cd = lm / sr]
	};

	struct LambertLight : public Light
	{
		explicit LambertLight(const char* _name = nullptr) :
			Light(Type::LAMBERT, _name)
		{}

		LambertLight(const ei::Vec3& _position, const ei::Vec3& _normal, const ei::Vec3& _intensity, const char* _name = nullptr) :
			Light(Type::LAMBERT, _name),
			position(_position),
			normal(_normal),
			intensity(_intensity)
		{}

		ei::Vec3 position;
		ei::Vec3 normal;
		ei::Vec3 intensity;		// [cd = lm / sr]
	};

	struct DirectionalLight : public Light
	{
		explicit DirectionalLight(const char* _name = nullptr) :
			Light(Type::DIRECTIONAL, _name)
		{}

		DirectionalLight(const ei::Vec3& _direction, const ei::Vec3& _irradiance, const char* _name = nullptr) :
			Light(Type::DIRECTIONAL, _name),
			direction(_direction),
			irradiance(_irradiance)
		{}

		ei::Vec3 direction;
		ei::Vec3 irradiance;	// [lm / m^2]
	};

	// A spot light with the intensity distribution:
	// I(t) = I0 * ((t - 1 + cos(halfAngle)) / cos(halfAngle))^falloff
	// with t is dot(spot.direction, query.direction)
	struct SpotLight : public Light
	{
		explicit SpotLight(const char* _name = nullptr) :
			Light(Type::SPOT, _name)
		{}

		SpotLight(const ei::Vec3& _position, const ei::Vec3& _direction, const ei::Vec3& _intensity, float _falloff, float _halfAngle, const char* _name = nullptr) :
			Light(Type::SPOT, _name),
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
	struct SkyLight : public Light
	{
		explicit SkyLight(const char* _name = nullptr) :
			Light(Type::SPOT, _name)
		{}

		SkyLight(const ei::Vec3& _sunDirection, float _turbidity, bool _aerialPerspective, const char* _name = nullptr) :
			Light(Type::SPOT, _name),
			sunDirection(_sunDirection),
			turbidity(_turbidity),
			aerialPerspective(_aerialPerspective)
		{}

		ei::Vec3 sunDirection;
		float turbidity;
		bool aerialPerspective;
	};

	struct GoniometricLight : public Light
	{
		explicit GoniometricLight(const char* _name = nullptr) :
			Light(Type::GONIOMETRIC, _name)
		{}

		GoniometricLight(const ei::Vec3& _position, const ei::Vec3& _intensityScale, const std::string& _intensityMap, const char* _name = nullptr) :
			Light(Type::GONIOMETRIC, _name),
			position(_position),
			intensityScale(_intensityScale),
			intensityMap(_intensityMap)
		{}

		ei::Vec3 position;
		ei::Vec3 intensityScale;
		std::string intensityMap;	// [cd = lm / sr]
	};

	struct EnvironmentLight : public Light
	{
		explicit EnvironmentLight(const char* _name = nullptr) :
			Light(Type::ENVIRONMENT, _name)
		{}

		explicit EnvironmentLight(const std::string& _radianceMap, const char* _name = nullptr) :
			Light(Type::ENVIRONMENT, _name),
			radianceMap(_radianceMap)
		{}

		std::string radianceMap;	// [cd / m^2]
	};
}
