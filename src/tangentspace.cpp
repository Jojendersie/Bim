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
		bool useTexCoords = needsAll;
		bool computeNormal = (_components & Property::NORMAL) || (needsAll && !(m_properties & Property::NORMAL));
		// Positions and texture coordinates are required for tangent space calculation.
		if(needsAll && !((m_properties & Property::TEXCOORD0) && (m_properties & Property::POSITION))) {
			if(m_properties & Property::POSITION)
			{
				// If not existent compute the normals as usual and the other things as defaults.
				useTexCoords = false;
			} else {
				std::cerr << "Can't compute tangent space. Texture coordinates missing.\n";
				return;
			}
		}

		// Zero all required ones
		if(computeNormal) swap(m_normals, std::vector<Vec3>(m_positions.size(), Vec3(0.0f)));
		if(needsAll) {
			swap(m_tangents, std::vector<Vec3>(m_positions.size(), Vec3(0.0f)));
			swap(m_bitangents, std::vector<Vec3>(m_positions.size(), Vec3(0.0f)));
		}
		if(_components & Property::QORMAL) swap(m_qormals, std::vector<Quaternion>(m_positions.size(), qidentity()));

		// Get tangent spaces on triangles and average them on vertex locations
		if(computeNormal || useTexCoords)
		for(size_t i = 0; i < m_triangles.size(); ++i)
		{
			Vec3 e0 = m_positions[m_triangles[i].y] - m_positions[m_triangles[i].x];
			Vec3 e1 = m_positions[m_triangles[i].z] - m_positions[m_triangles[i].x];
			Vec3 e2 = m_positions[m_triangles[i].z] - m_positions[m_triangles[i].y];
			Vec3 triNormal, triTangent, triBitangent;
			triNormal = normalize(cross(e0, e1));
			eiAssert(all(triNormal == triNormal), "NaN in normal computation!");
			if(useTexCoords) {
				Vec2 uva = m_texCoords0[m_triangles[i].y] - m_texCoords0[m_triangles[i].x];
				Vec2 uvb = m_texCoords0[m_triangles[i].z] - m_texCoords0[m_triangles[i].x];
				float det = uva.x * uvb.y - uva.y * uvb.x; // may swap the sign
				if(det == 0.0f) det = 1.0f;
				triTangent = (uvb.y * e0 - uva.y * e1) / det;
				triBitangent = (uva.x * e1 - uvb.x * e0) / det;
				// Try to recover direction if it got NaN
				bool invalidTangent = !all(triTangent == triTangent) || len(triTangent) < 1e-10f;
				bool invalidBitangent = !all(triBitangent == triBitangent) || len(triBitangent) < 1e-10f;
				if(invalidTangent && invalidBitangent)
				{
					// Create a random orthonormal basis (no uv given)
					triTangent = Vec3(1.0f, triNormal.x, 0.0f);
					triBitangent = Vec3(0.0f, triNormal.z, 1.0f);
				} else if(invalidTangent)
					triTangent = cross(triBitangent, triNormal) * det;
				else if(invalidBitangent)
						triBitangent = cross(triNormal, triTangent) * det;
				if(!ei::orthonormalize(triNormal, triTangent, triBitangent))
					triBitangent = cross(triNormal, triTangent);
				eiAssert(all(triTangent == triTangent), "NaN in tangent computation!");
				eiAssert(all(triBitangent == triBitangent), "NaN in bitangent computation!");
				eiAssert(approx(len(triTangent), 1.0f, 1e-4f), "Computed tangent has a wrong length!");
				eiAssert(approx(len(triBitangent), 1.0f, 1e-4f), "Computed bitangent has a wrong length!");
			}
			float lenE0 = len(e0), lenE1 = len(e1), lenE2 = len(e2);
			float weight = acos(saturate(dot(e0, e1) / (lenE0 * lenE1)));
			if(computeNormal) m_normals[m_triangles[i].x] += triNormal * weight;
			if(useTexCoords) m_tangents[m_triangles[i].x] += triTangent * weight;
			if(useTexCoords) m_bitangents[m_triangles[i].x] += triBitangent * weight;
			weight = acos(saturate(-dot(e0, e2) / (lenE0 * lenE2)));
			if(computeNormal) m_normals[m_triangles[i].y] += triNormal * weight;
			if(useTexCoords) m_tangents[m_triangles[i].y] += triTangent * weight;
			if(useTexCoords) m_bitangents[m_triangles[i].y] += triBitangent * weight;
			weight = acos(saturate(dot(e1, e2) / (lenE1 * lenE2)));
			if(computeNormal) m_normals[m_triangles[i].z] += triNormal * weight;
			if(useTexCoords) m_tangents[m_triangles[i].z] += triTangent * weight;
			if(useTexCoords) m_bitangents[m_triangles[i].z] += triBitangent * weight;
		}

		// Orthonormalize
		if(useTexCoords)
			for(size_t i = 0; i < m_normals.size(); ++i)
				ei::orthonormalize(m_normals[i], m_tangents[i], m_bitangents[i]);
		else if(computeNormal)
			for(size_t i = 0; i < m_normals.size(); ++i)
				m_normals[i] = normalize(m_normals[i]);

		// Generate some "random" tangent spaces without the need of texture coordinates.
		if(needsAll && !useTexCoords)
		{
			for(size_t i = 0; i < m_normals.size(); ++i)
			{
				Mat3x3 m = ei::basis(m_normals[i]);
				m_tangents[i] = ei::Vec3(m.m10, m.m11, m.m12);
				m_bitangents[i] = ei::Vec3(m.m20, m.m21, m.m22);
			}
		}

		// Compute qormals by conversion
		if(_components & Property::QORMAL)
			for(size_t i = 0; i < m_normals.size(); ++i)
				m_qormals[i] = Quaternion(m_normals[i], m_tangents[i], m_bitangents[i]);

		// Discard all the undesired properties for size reasons.
		if(!(_components & Property::NORMAL) && !(m_properties & Property::NORMAL)) swap(m_normals, std::vector<Vec3>());
		if(!(_components & Property::TANGENT) && !(m_properties & Property::TANGENT)) swap(m_tangents, std::vector<Vec3>());
		if(!(_components & Property::BITANGENT) && !(m_properties & Property::BITANGENT)) swap(m_bitangents, std::vector<Vec3>());
		if(!(_components & Property::QORMAL) && !(m_properties & Property::QORMAL)) swap(m_qormals, std::vector<Quaternion>());

		// Update flags
		m_properties = Property::Val(m_properties | _components);
	}

	void Chunk::flipNormals()
	{
		for(size_t i = 0; i < m_normals.size(); ++i)
			m_normals[i] = -m_normals[i];

		// TODO: flip Qormals too
	}

	void Chunk::unifyQormals()
	{
	}

} // namespace bim