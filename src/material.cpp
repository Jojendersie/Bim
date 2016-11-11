#include "material.hpp"

namespace bim {

	Material::Material(std::string _name, std::string _type) :
		m_name(move(_name)),
		m_type(move(_type))
	{
	}

	void Material::setType(std::string _type)
	{
		m_type = move(_type);
	}

	const std::string* Material::getTexture(const std::string& _name) const
	{
		auto it = m_textureNames.find(_name);
		if(it != m_textureNames.end())
			return &it->second;
		return nullptr;
	}

	const float Material::get(const std::string & _name, const float _default) const
	{
		auto it = m_values.find(_name);
		if(it != m_values.end())
			return *reinterpret_cast<const float*>(&it->second);
		return _default;
	}

	const ei::Vec2 & Material::get(const std::string & _name, const ei::Vec2 & _default) const
	{
		auto it = m_values.find(_name);
		if(it != m_values.end())
			return *reinterpret_cast<const ei::Vec2*>(&it->second);
		return _default;
	}

	const ei::Vec3 & Material::get(const std::string & _name, const ei::Vec3 & _default) const
	{
		auto it = m_values.find(_name);
		if(it != m_values.end())
			return *reinterpret_cast<const ei::Vec3*>(&it->second);
		return _default;
	}

	const ei::Vec4 & Material::get(const std::string & _name, const ei::Vec4 & _default) const
	{
		auto it = m_values.find(_name);
		if(it != m_values.end())
			return it->second;
		return _default;
	}

	void Material::set(const std::string & _name, const float _value)
	{
		m_values[_name] = ei::Vec4(_value, 0.0f, 0.0f, 0.0f);
	}

	void Material::set(const std::string & _name, const ei::Vec2 & _value)
	{
		m_values[_name] = ei::Vec4(_value, 0.0f, 0.0f);
	}

	void Material::set(const std::string & _name, const ei::Vec3 & _value)
	{
		m_values[_name] = ei::Vec4(_value, 0.0f);
	}

	void Material::set(const std::string & _name, const ei::Vec4 & _value)
	{
		m_values[_name] = _value;
	}
	
	void Material::setTexture(const std::string& _name, std::string _textureFile)
	{
		m_textureNames[_name] = move(_textureFile);
	}

} // namespace bim