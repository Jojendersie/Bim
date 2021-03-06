#pragma once

#include <ei/vector.hpp>
#include <unordered_map>

namespace bim {

	// Generic material class. It manages arbitrary properties without assumptions
	// for the parametrization.
	// A property from the scene-json file is either read as texture name (string)
	// or as vector value.
	class Material
	{
	public:
		Material() = default;
		Material(std::string _name, std::string _type);

		const std::string& getName() const { return m_name; }
		// Set and get a generic type name. Since this library makes no
		// assumptions the semantic and the required/optional attributes must be
		// given by the documentation of the target project.
		void setType(std::string _type);
		const std::string& getType() const { return m_type; }
		// Get names for textures of properties.
		// If the function returns nullptr use the scalar value.
		const std::string* getTexture(const std::string& _name) const;
		// Get the value of some property. If the property does not exist return the
		// default instead.
		float get(const std::string& _name, const float _default = 0.0f) const;
		ei::Vec2 get(const std::string& _name, const ei::Vec2& _default = ei::Vec2(0.0f)) const;
		ei::Vec3 get(const std::string& _name, const ei::Vec3& _default = ei::Vec3(0.0f)) const;
		ei::Vec4 get(const std::string& _name, const ei::Vec4& _default = ei::Vec4(0.0f)) const;
		// Add or replace a value
		void set(const std::string& _name, const float _value);
		void set(const std::string& _name, const ei::Vec2& _value);
		void set(const std::string& _name, const ei::Vec3& _value);
		void set(const std::string& _name, const ei::Vec4& _value);
		// Check if a specific attribute exists (in textures or values).
		bool has(const std::string& _name) const;
		// Add or replace a texture value
		void setTexture(const std::string& _name, std::string _textureFile);
	private:
		std::unordered_map<std::string, std::string> m_textureNames;
		struct MultiValue
		{
			ei::Vec4 values;
			int numComponents;
		};
		std::unordered_map<std::string, MultiValue> m_values;
		std::string m_name;
		std::string m_type;
		friend class BinaryModel;
	};

} // namespace bim