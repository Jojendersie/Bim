#include "bim/material.hpp"

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

	float Material::get(const std::string & _name, const float _default) const
	{
		auto it = m_values.find(_name);
		if(it != m_values.end())
			return *reinterpret_cast<const float*>(&it->second.values);
		return _default;
	}

	ei::Vec2 Material::get(const std::string & _name, const ei::Vec2 & _default) const
	{
		auto it = m_values.find(_name);
		if(it != m_values.end())
		{
			ei::Vec2 res(it->second.values);
			// Fill missing components with the default.
			for(int i = it->second.numComponents; i < 2; ++i)
				res[i] = _default[i];
			return res;
		}
		return _default;
	}

	ei::Vec3 Material::get(const std::string & _name, const ei::Vec3 & _default) const
	{
		auto it = m_values.find(_name);
		if(it != m_values.end())
		{
			ei::Vec3 res(it->second.values);
			// Fill missing components with the default.
			for(int i = it->second.numComponents; i < 3; ++i)
				res[i] = _default[i];
			return res;
		}
		return _default;
	}

	ei::Vec4 Material::get(const std::string & _name, const ei::Vec4 & _default) const
	{
		auto it = m_values.find(_name);
		if(it != m_values.end())
		{
			ei::Vec4 res = it->second.values;
			// Fill missing components with the default.
			for(int i = it->second.numComponents; i < 4; ++i)
				res[i] = _default[i];
			return res;
		}
		return _default;
	}

	void Material::set(const std::string & _name, const float _value)
	{
		m_values[_name] = MultiValue{ei::Vec4(_value, 0.0f, 0.0f, 0.0f), 1};
	}

	void Material::set(const std::string & _name, const ei::Vec2 & _value)
	{
		m_values[_name] = MultiValue{ei::Vec4(_value, 0.0f, 0.0f), 2};
	}

	void Material::set(const std::string & _name, const ei::Vec3 & _value)
	{
		m_values[_name] = MultiValue{ei::Vec4(_value, 0.0f), 3};
	}

	void Material::set(const std::string & _name, const ei::Vec4 & _value)
	{
		m_values[_name] = MultiValue{_value, 4};
	}

	bool Material::has(const std::string & _name) const
	{
		if( m_values.find(_name) != m_values.end()) return true;
		if( m_textureNames.find(_name) != m_textureNames.end()) return true;
		return false;
	}

	void Material::setTexture(const std::string& _name, std::string _textureFile)
	{
		m_textureNames[_name] = move(_textureFile);
	}

} // namespace bim