#include "bim.hpp"

namespace bim {
	
	void Chunk::addVertex(VertexPropertyMap* _properties)
	{
		if(m_properties & Property::POSITION)
		{
			if(_properties->position)
				m_normals.push_back(*_properties->position);
			else m_normals.push_back(ei::Vec3(0.0f));
		}
		if(m_properties & Property::NORMAL)
		{
			if(_properties->normal)
				m_normals.push_back(*_properties->normal);
			else m_normals.push_back(ei::Vec3(0.0f, 1.0f, 0.0f));
		}
		if(m_properties & Property::TANGENT)
		{
			if(_properties->tangent)
				m_tangents.push_back(*_properties->tangent);
			else m_tangents.push_back(ei::Vec3(1.0f, 0.0f, 0.0f));
		}
		if(m_properties & Property::BITANGENT)
		{
			if(_properties->bitangent)
				m_bitangents.push_back(*_properties->bitangent);
			else m_bitangents.push_back(ei::Vec3(0.0f, 0.0f, 1.0f));
		}
		if(m_properties & Property::QORMAL)
		{
			if(_properties->qormal)
				m_qormals.push_back(*_properties->qormal);
			else m_qormals.push_back(ei::qidentity());
		}
		if(m_properties & Property::TEXCOORD0)
		{
			if(_properties->texCoord0)
				m_texCoords0.push_back(*_properties->texCoord0);
			else m_texCoords0.push_back(ei::Vec2(0.0f));
		}
		if(m_properties & Property::TEXCOORD1)
		{
			if(_properties->texCoord1)
				m_texCoords1.push_back(*_properties->texCoord1);
			else m_texCoords1.push_back(ei::Vec2(0.0f));
		}
		if(m_properties & Property::TEXCOORD2)
		{
			if(_properties->texCoord2)
				m_texCoords2.push_back(*_properties->texCoord2);
			else m_texCoords2.push_back(ei::Vec2(0.0f));
		}
		if(m_properties & Property::TEXCOORD3)
		{
			if(_properties->texCoord3)
				m_texCoords3.push_back(*_properties->texCoord3);
			else m_texCoords3.push_back(ei::Vec2(0.0f));
		}
		if(m_properties & Property::COLOR)
		{
			if(_properties->color)
				m_colors.push_back(*_properties->color);
			else m_colors.push_back(0);
		}
	}
		
	void Chunk::addTriangle(const ei::UVec3& _indices, uint _material)
	{
		m_triangles.push_back(ei::UVec4(_indices, _material));
	}

} // namespace bim