#include "material.hpp"

namespace bim {

	Material::Material(std::string _name) :
		m_name(move(_name))
	{
	}

	const char* Material::getTexture(const std::string& _name) const
	{
		auto it = m_textureNames.find(_name);
		if(it != m_textureNames.end())
			return it->second.c_str();
		return nullptr;
	}

	const ei::Vec4& Material::get(const std::string& _name) const
	{
		auto it = m_values.find(_name);
		if(it != m_values.end())
			return it->second;
		static ei::Vec4 nullVec(0.0f);
		return nullVec;
	}

	void Material::set(const std::string& _name, const ei::Vec4& _value)
	{
		m_values[_name] = _value;
	}
	
	void Material::setTexture(const std::string& _name, std::string _textureFile)
	{
		m_textureNames[_name] = move(_textureFile);
	}

} // namespace bim