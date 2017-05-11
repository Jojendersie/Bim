#pragma once

#include <ei/vector.hpp>
#include <string>
#include "../deps/EnumConverter.h"

namespace bim {

	struct Camera
	{
		enum class Type {
			PERSPECTIVE,
			ORTHOGRAPHIC,
			FOCUS,

			NUM_TYPES
		};
		ENUM_CONVERT_FUNC(Type)

		explicit Camera(Type _type, std::string _name = "") :
			type(_type),
			velocity(1.0f)
		{
			static int s_genericCameraName = 0;
			if(!_name.empty()) name = move(_name);
			else name = s_genericCameraName++;
		}
		virtual ~Camera() = default;

		const Type type;
		std::string name;
		float velocity;
	};

	struct PerspectiveCamera : public Camera
	{
		explicit PerspectiveCamera(std::string _name = "") :
			Camera(Type::PERSPECTIVE, move(_name))
		{}

		PerspectiveCamera(const ei::Vec3& _position, const ei::Vec3& _lookAt, const ei::Vec3& _up, float _fov, std::string _name = "") :
			Camera(Type::PERSPECTIVE, move(_name)),
			position(_position),
			lookAt(_lookAt),
			up(_up),
			verticalFOV(_fov)
		{}

		ei::Vec3 position;
		ei::Vec3 lookAt;
		ei::Vec3 up;
		float verticalFOV;	// Vertical field of view in radiants
	};

	struct OrthographicCamera : public Camera
	{
		explicit OrthographicCamera(std::string _name = "") :
			Camera(Type::ORTHOGRAPHIC, move(_name))
		{}

		OrthographicCamera(const ei::Vec3& _position, const ei::Vec3& _lookAt, const ei::Vec3& _up, float _left, float _right, float _bottom, float _top, float _near, float _far, std::string _name = "") :
			Camera(Type::ORTHOGRAPHIC, move(_name)),
			position(_position),
			lookAt(_lookAt),
			up(_up),
			left(_left),
			right(_right),
			bottom(_bottom),
			top(_top),
			near(_near),
			far(_far)
		{}

		ei::Vec3 position;
		ei::Vec3 lookAt;
		ei::Vec3 up;
		float left;
		float right;
		float bottom;
		float top;
		float near;
		float far;
	};

	struct FocusCamera : public Camera
	{
		explicit FocusCamera(std::string _name = "") :
			Camera(Type::FOCUS, move(_name))
		{}

		FocusCamera(const ei::Vec3& _position, const ei::Vec3& _lookAt, const ei::Vec3& _up, float _focalLength, float _focusDistance, float _sensorSize, float _aperture, std::string _name = "") :
			Camera(Type::FOCUS, move(_name)),
			position(_position),
			lookAt(_lookAt),
			up(_up),
			focalLength(_focalLength),
			focusDistance(_focusDistance),
			sensorSize(_sensorSize),
			aperture(_aperture)
		{}

		ei::Vec3 position;
		ei::Vec3 lookAt;
		ei::Vec3 up;
		float focalLength;
		float focusDistance;
		float sensorSize;
		float aperture;
	};
}
