#pragma once

#include <ei/vector.hpp>
#include <unordered_map>

namespace bim {

	// Generic material class. It manages arbitrary properties without assumptions
	// for the parametrization.
	// A property from the scene-json file is either read as texture name (string)
	// or as vector value. Values are always stored as Vec4 independent of their
	// type or dimensionality. Unused parts are filled with zeros.
	class Material
	{
	public:
		Material() = default;
		Material(std::string _name);
		// Get names for textures of properties.
		// If the function returns nullptr use the scalar value.
		const char* getTexture(const std::string& _name) const;
		// Get the value of some property.
		const ei::Vec4& get(const std::string& _name) const;
		const std::string& getName() const { return m_name; }
		// Add or replace a value
		void set(const std::string& _name, const ei::Vec4& _value);
		// Add or replace a texture value
		void setTexture(const std::string& _name, std::string _textureFile);
	private:
		std::unordered_map<std::string, std::string> m_textureNames;
		std::unordered_map<std::string, ei::Vec4> m_values;
		std::string m_name;
		friend class BinaryModel;
	};

} // namespace bim