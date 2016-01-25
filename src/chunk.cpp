#include "bim.hpp"

namespace bim {

	Chunk::Chunk() :
		m_address(0),
		m_properties(Property::DONT_CARE),
		m_numTrianglesPerLeaf(0),
		m_numTreeLevels(0)
	{
	}

	void Chunk::addVertex(const FullVertex& _properties)
	{
		if(m_positions.empty())
			m_boundingBox.min = m_boundingBox.max = _properties.position;
		else {
			m_boundingBox.min = min(_properties.position, m_boundingBox.min);
			m_boundingBox.max = max(_properties.position, m_boundingBox.max);
		}
		m_positions.push_back(_properties.position);
		if(m_properties & Property::NORMAL)
			m_normals.push_back(_properties.normal);
		if(m_properties & Property::TANGENT)
			m_tangents.push_back(_properties.tangent);
		if(m_properties & Property::BITANGENT)
			m_bitangents.push_back(_properties.bitangent);
		if(m_properties & Property::QORMAL)
			m_qormals.push_back(_properties.qormal);
		if(m_properties & Property::TEXCOORD0)
			m_texCoords0.push_back(_properties.texCoord0);
		if(m_properties & Property::TEXCOORD1)
			m_texCoords1.push_back(_properties.texCoord1);
		if(m_properties & Property::TEXCOORD2)
			m_texCoords2.push_back(_properties.texCoord2);
		if(m_properties & Property::TEXCOORD3)
			m_texCoords3.push_back(_properties.texCoord3);
		if(m_properties & Property::COLOR)
			m_colors.push_back(_properties.color);
	}
		
	void Chunk::addTriangle(const ei::UVec3& _indices, uint32 _material)
	{
		m_triangles.push_back(_indices);
		if(m_properties & Property::TRIANGLE_MAT)
			m_triangleMaterials.push_back(_material);
	}

	static ei::Vec3 denoise(const ei::Vec3& _x)
	{
		 return *reinterpret_cast<const ei::Vec3*>(&(*reinterpret_cast<const ei::UVec3*>(&_x) & 0xfffffff0));
	}
	static ei::Vec2 denoise(const ei::Vec2& _x)
	{
		return *reinterpret_cast<const ei::Vec2*>(&(*reinterpret_cast<const ei::UVec2*>(&_x) & 0xfffffff0));
	}

	void Chunk::removeRedundantVertices()
	{
		invalidateHierarchy();

		std::unordered_map<FullVertex, uint> vertexToIndex;
		uint index = 0;
		size_t numVertices = m_positions.size();
		// Initialize the index mapping to index->self
		std::vector<uint> indexToIndex(numVertices);
		for(size_t i = 0; i < numVertices; ++i)
			indexToIndex[i] = (uint)i;
		// For each vertex
		for(size_t i = 0; i < numVertices; ++i)
		{
			// Create a key
			FullVertex key;
			// Round properties (denoise) in vertices (otherwise hashing will miss many)
			key.position = denoise(m_positions[i]);
			if(!m_normals.empty()) key.normal = normalize(denoise(m_normals[i]));
			if(!m_tangents.empty()) key.tangent = normalize(denoise(m_tangents[i]));
			if(!m_bitangents.empty()) key.bitangent = normalize(denoise(m_bitangents[i]));
			if(!m_qormals.empty()) key.qormal = m_qormals[i]; // TODO denoise
			if(!m_texCoords0.empty()) key.texCoord0 = denoise(m_texCoords0[i]);
			if(!m_texCoords1.empty()) key.texCoord1 = denoise(m_texCoords1[i]);
			if(!m_texCoords2.empty()) key.texCoord2 = denoise(m_texCoords2[i]);
			if(!m_texCoords3.empty()) key.texCoord3 = denoise(m_texCoords3[i]);
			if(!m_colors.empty()) key.color = m_colors[i];
			// Get the index
			auto it = vertexToIndex.find(key);
			if(it == vertexToIndex.end())
			{
				// This is a new vertex -> keep (but move to index)
				vertexToIndex[key] = index;
				indexToIndex[i] = index;
				// Move data from i -> index
				m_positions[index] = m_positions[i];
				if(!m_normals.empty()) m_normals[index] = m_normals[i];
				if(!m_tangents.empty()) m_tangents[index] = m_tangents[i];
				if(!m_bitangents.empty()) m_bitangents[index] = m_bitangents[i];
				if(!m_qormals.empty()) m_qormals[index] = m_qormals[i];
				if(!m_texCoords0.empty()) m_texCoords0[index] = m_texCoords0[i];
				if(!m_texCoords1.empty()) m_texCoords1[index] = m_texCoords1[i];
				if(!m_texCoords2.empty()) m_texCoords2[index] = m_texCoords2[i];
				if(!m_texCoords3.empty()) m_texCoords3[index] = m_texCoords3[i];
				if(!m_colors.empty()) m_colors[index] = m_colors[i];
				++index;
			} else {
				indexToIndex[i] = it->second;
			}
		}
		// All vertices we kept are moved to a block of size 'index'. The remaining
		// part can be removed.
		m_positions.resize(index);
		if(!m_normals.empty()) m_normals.resize(index);
		if(!m_tangents.empty()) m_tangents.resize(index);
		if(!m_bitangents.empty()) m_bitangents.resize(index);
		if(!m_qormals.empty()) m_qormals.resize(index);
		if(!m_texCoords0.empty()) m_texCoords0.resize(index);
		if(!m_texCoords1.empty()) m_texCoords1.resize(index);
		if(!m_texCoords2.empty()) m_texCoords2.resize(index);
		if(!m_texCoords3.empty()) m_texCoords3.resize(index);
		if(!m_colors.empty()) m_colors.resize(index);

		// Rebuild index buffer
		for(size_t i = 0; i < m_triangles.size(); ++i)
		{
			m_triangles[i].x = indexToIndex[m_triangles[i].x];
			m_triangles[i].y = indexToIndex[m_triangles[i].y];
			m_triangles[i].z = indexToIndex[m_triangles[i].z];
		}
	}

	void Chunk::rebuildHierarchy(BuildMethod _method, uint _numTrianglesPerLeaf)
	{
		m_numTrianglesPerLeaf = _numTrianglesPerLeaf;
		switch(_method)
		{
		case BuildMethod::KD_TREE:
			buildBVH_kdtree();
			break;
		case BuildMethod::SAH:
			buildBVH_SAHsplit();
			break;
		}

		m_numTreeLevels = remapNodePointers(0, 0, 0);
		m_properties = Property::Val(m_properties | Property::HIERARCHY);
	}

	void Chunk::addProperty(Property::Val _property)
	{
		if(!(m_properties & _property))
		{
			switch(_property)
			{
			case Property::NORMAL: swap(m_normals, std::vector<ei::Vec3>(m_positions.size(), FullVertex().position)); break;
			case Property::TANGENT: swap(m_tangents, std::vector<ei::Vec3>(m_positions.size(), FullVertex().tangent)); break;
			case Property::BITANGENT: swap(m_bitangents, std::vector<ei::Vec3>(m_positions.size(), FullVertex().bitangent)); break;
			case Property::QORMAL: swap(m_qormals, std::vector<ei::Quaternion>(m_positions.size(), FullVertex().qormal)); break;
			case Property::TEXCOORD0: swap(m_texCoords0, std::vector<ei::Vec2>(m_positions.size(), FullVertex().texCoord0)); break;
			case Property::TEXCOORD1: swap(m_texCoords0, std::vector<ei::Vec2>(m_positions.size(), FullVertex().texCoord1)); break;
			case Property::TEXCOORD2: swap(m_texCoords0, std::vector<ei::Vec2>(m_positions.size(), FullVertex().texCoord2)); break;
			case Property::TEXCOORD3: swap(m_texCoords0, std::vector<ei::Vec2>(m_positions.size(), FullVertex().texCoord3)); break;
			case Property::COLOR: swap(m_colors, std::vector<uint32>(m_positions.size(), FullVertex().color)); break;
			case Property::TRIANGLE_MAT: swap(m_triangleMaterials, std::vector<uint32>(m_triangles.size(), 0)); break;
			case Property::AABOX_BVH: swap(m_aaBoxes, std::vector<ei::Box>(m_hierarchy.size())); break;
			case Property::OBOX_BVH: //swap(m_aaBoxes, std::vector<ei::Box>(m_hierarchy.size())); break;
			case Property::SPHERE_BVH: //swap(m_aaBoxes, std::vector<ei::Box>(m_hierarchy.size())); break;
			case Property::NDF_SGGX: swap(m_nodeNDFs, std::vector<SGGX>(m_hierarchy.size())); break;
			default: return;
			}
			m_properties = Property::Val(m_properties | _property);
		}
	}

	void Chunk::invalidateHierarchy()
	{
		// Remove all data, it needs to be recomputed anyway
		m_hierarchy.clear();
		m_hierarchyLeaves.clear();
		m_aaBoxes.clear();
		m_nodeNDFs.clear();
		m_properties = Property::Val(m_properties
			& ~(Property::HIERARCHY | Property::AABOX_BVH 
			  | Property::OBOX_BVH | Property::SPHERE_BVH | Property::NDF_SGGX));
		m_numTreeLevels = 0;
	}

	// ********************************************************************* //
	Chunk::FullVertex::FullVertex() :
		position(0.0f),
		normal(0.0f, 1.0f, 0.0),
		tangent(1.0f, 0.0f, 0.0f),
		bitangent(0.0f, 0.0f, 1.0f),
		qormal(ei::qidentity()),
		texCoord0(0.0f),
		texCoord1(0.0f),
		texCoord2(0.0f),
		texCoord3(0.0f),
		color(0)
	{
	}

	bool Chunk::FullVertex::operator == (const FullVertex& _rhs) const
	{
		if(any(position != _rhs.position)) return false;
		if(any(normal != _rhs.normal)) return false;
		if(any(tangent != _rhs.tangent)) return false;
		if(any(bitangent != _rhs.bitangent)) return false;
		if(qormal != _rhs.qormal) return false;
		if(any(texCoord0 != _rhs.texCoord0)) return false;
		if(any(texCoord1 != _rhs.texCoord1)) return false;
		if(any(texCoord2 != _rhs.texCoord2)) return false;
		if(any(texCoord3 != _rhs.texCoord3)) return false;
		if(color != _rhs.color) return false;
		return true;
	}

} // namespace bim