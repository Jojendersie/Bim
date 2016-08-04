#include "chunk.hpp"
#include <iostream>

using namespace ei;

namespace bim {

	void Chunk::computeTangentSpace(Property::Val _components)
	{
		// Either compute normals only or compute the entire tangent space,
		// orthonormalize and discard the unwanted.
		// For quaternions the entire space is computed and then converted.
		bool needsAll = (_components & Property::QORMAL) || (_components & Property::TANGENT) || (m_properties & Property::BITANGENT);
		// Positions and texture coordinates are required for tangent space calculation.
		if(needsAll && !((m_properties & Property::TEXCOORD0) && (m_properties & Property::POSITION))) {
			std::cerr << "Can't compute tangent space. Texture coordinates missing.\n";
			return;
		}

		// Zero all required ones
		swap(m_normals, std::vector<Vec3>(m_positions.size(), Vec3(0.0f)));
		if(needsAll) {
			swap(m_tangents, std::vector<Vec3>(m_positions.size(), Vec3(0.0f)));
			swap(m_bitangents, std::vector<Vec3>(m_positions.size(), Vec3(0.0f)));
		}
		if(_components & Property::QORMAL) swap(m_qormals, std::vector<Quaternion>(m_positions.size(), qidentity()));

		// Get tangent spaces on triangles and average them on vertex locations
		for(size_t i = 0; i < m_triangles.size(); ++i)
		{
			Vec3 e0 = m_positions[m_triangles[i].y] - m_positions[m_triangles[i].x];
			Vec3 e1 = m_positions[m_triangles[i].z] - m_positions[m_triangles[i].x];
			Vec3 e2 = m_positions[m_triangles[i].z] - m_positions[m_triangles[i].y];
			Vec3 triNormal, triTangent, triBitangent;
			triNormal = normalize(cross(e0, e1));
			if(needsAll) {
				Vec2 uva = m_texCoords0[m_triangles[i].y] - m_texCoords0[m_triangles[i].x];
				Vec2 uvb = m_texCoords0[m_triangles[i].z] - m_texCoords0[m_triangles[i].x];
				float det = uva.x * uvb.y - uva.y * uvb.x; // may swap the sign
				triTangent = (uvb.y * e0 - uva.y * e1) / det;
				triBitangent = (uva.x * e1 - uvb.x * e0) / det;
				ei::orthonormalize(triNormal, triTangent, triBitangent);
				//triTangent = normalize(uva.y * e1 - uvb.y * e0);
				//triBitangent = cross(triNormal, triTangent);
				//if(uva.x * uvb.y - uva.y * uvb.x < 0.0f) // "normal" of the uv coordinates
				//	triBitangent = -triBitangent;
			}
			float lenE0 = len(e0), lenE1 = len(e1), lenE2 = len(e2);
			float weight = acos(saturate(dot(e0, e1) / (lenE0 * lenE1)));
			m_normals[m_triangles[i].x] += triNormal * weight;
			if(needsAll) m_tangents[m_triangles[i].x] += triTangent * weight;
			if(needsAll) m_bitangents[m_triangles[i].x] += triBitangent * weight;
			weight = acos(saturate(-dot(e0, e2) / (lenE0 * lenE2)));
			m_normals[m_triangles[i].y] += triNormal * weight;
			if(needsAll) m_tangents[m_triangles[i].y] += triTangent * weight;
			if(needsAll) m_bitangents[m_triangles[i].y] += triBitangent * weight;
			weight = acos(saturate(dot(e1, e2) / (lenE1 * lenE2)));
			m_normals[m_triangles[i].z] += triNormal * weight;
			if(needsAll) m_tangents[m_triangles[i].z] += triTangent * weight;
			if(needsAll) m_bitangents[m_triangles[i].z] += triBitangent * weight;
		}

		// Orthonormalize
		if(needsAll)
			for(size_t i = 0; i < m_positions.size(); ++i)
				ei::orthonormalize(m_normals[i], m_tangents[i], m_bitangents[i]);
		else for(size_t i = 0; i < m_positions.size(); ++i)
			m_normals[i] = normalize(m_normals[i]);
		// Compute qormals by conversion
		if(_components & Property::QORMAL)
			for(size_t i = 0; i < m_positions.size(); ++i)
				m_qormals[i] = Quaternion(m_normals[i], m_tangents[i], m_bitangents[i]);

		// Discard all the undesired properties for size reasons.
		if(!(_components & Property::NORMAL)) swap(m_normals, std::vector<Vec3>());
		if(!(_components & Property::TANGENT)) swap(m_tangents, std::vector<Vec3>());
		if(!(_components & Property::BITANGENT)) swap(m_bitangents, std::vector<Vec3>());
		if(!(_components & Property::QORMAL)) swap(m_qormals, std::vector<Quaternion>());

		// Update flags (remove old, set new)
		m_properties = Property::Val(m_properties & ~(Property::NORMAL
			| Property::TANGENT | Property::BITANGENT | Property::QORMAL));
		m_properties = Property::Val(m_properties | _components);
	}

	void Chunk::unifyQormals()
	{
	}

} // namespace bim