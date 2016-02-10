#include "bim.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>

namespace bim {

	struct SectionHeader
	{
		uint32 type;	// BinaryModel::Property::Val + extra values
		uint64 size;	// Block size of following data
	};

	// Non powers of 2 are free to use (others are reserved for Property::...)
	const int META_SECTION = 0x0;
	const int CHUNK_SECTION = 0x3;
	const int MATERIAL_REFERENCE = 0x5;
	const int HIERARCHY_LEAVES = 0x08000001;
	const int CHUNK_META_SECTION = 0x6;

	struct MetaSection
	{
		ei::IVec3 numChunks;	// Number of stored chunks
		ei::Box boundingBox;	// Entire scene bounding box
	};

	struct ChunkMetaSection
	{
		ei::Box boundingBox;
		uint m_numTrianglesPerLeaf;
		uint m_numTreeLevels;
	};

	bool BinaryModel::load(const char* _bimFile, const char* _envFile, Property::Val _requiredProperties, Property::Val _optionalProperties, bool _loadAll)
	{
		if(!loadEnv(_envFile)) return false;

		m_file.open(_bimFile, std::ios_base::binary);
		if(m_file.fail()) {
			std::cerr << "Cannot open scene file!\n";
			return false;
		}
		SectionHeader header;
	
		// Analyse file:
		//  * Is the requested information available?
		//  * Get the jump addresses of each chunk.
		m_file.read(reinterpret_cast<char*>(&header), sizeof(SectionHeader));
		if((header.type != META_SECTION) || (header.size != sizeof(MetaSection))) {
			std::cerr << "Invalid file. Meta-section not found!\n";
			return false;
		}

		MetaSection meta;
		m_file.read(reinterpret_cast<char*>(&meta), sizeof(MetaSection));
		m_loadAll = _loadAll;
		// Make sure at least positions and triangles are available
		m_requestedProps = Property::Val(_requiredProperties | Property::POSITION | Property::TRIANGLE_IDX);
		m_optionalProperties = _optionalProperties;
		m_numChunks = meta.numChunks;
		m_dimScale = ei::IVec3(1, m_numChunks.x, m_numChunks.x * m_numChunks.y);
		m_boundingBox = meta.boundingBox;
	
		m_chunks.clear();
		m_chunkStates.clear();
		Chunk emptyChunk;
		emptyChunk.m_properties = Property::DONT_CARE;
		while(m_file.read(reinterpret_cast<char*>(&header), sizeof(SectionHeader)))
		{
			if(header.type == CHUNK_SECTION)
			{
				emptyChunk.m_address = m_file.tellg();
				m_chunks.push_back(emptyChunk);
				m_chunkStates.push_back(ChunkState::EMPTY);
				m_file.seekg(header.size, std::ios_base::cur);
			} else if(header.type == MATERIAL_REFERENCE)
			{
				uint32 num;
				m_file.read(reinterpret_cast<char*>(&num), sizeof(uint32));
				char buf[64];
				for(uint i = 0; i < num; ++i)
				{
					m_file.read(buf, 64);
					// Find the referenced material
					uint index = 0;
					for(auto& mat : m_materials)
					{
						if(strcmp(mat.getName().c_str(), buf) == 0)
							break;
						index++;
					}
					m_materialIndirection.push_back(index);
				}
			} else
				m_file.seekg(header.size, std::ios_base::cur);
		}

		// Validation
		if(m_chunks.size() != prod(m_numChunks))
		{
			std::cerr << "Missing chunks!\n";
			return false;
		}

		// If there where no material references stored build a dummy map
		// to all existing materials.
		if(m_materialIndirection.empty())
		{
			for(size_t i = 0; i < m_materials.size(); ++i)
				m_materialIndirection.push_back((uint)i);
		}

		return true;
	}

	void BinaryModel::store(const char* _bimFile, const char* _envFile)
	{
		std::ofstream file(_bimFile, std::ios_base::binary | std::ios_base::out);
		if(m_file.bad()) {std::cerr << "Cannot open file for writing!\n"; return;}
		SectionHeader header;
	
		header.type = META_SECTION;
		header.size = sizeof(MetaSection);
		file.write(reinterpret_cast<char*>(&header), sizeof(SectionHeader));
	
		MetaSection meta;
		refreshBoundingBox();
		meta.numChunks = m_numChunks;
		meta.boundingBox = m_boundingBox;
		file.write(reinterpret_cast<char*>(&meta), sizeof(MetaSection));

		header.type = MATERIAL_REFERENCE;
		header.size = m_materialIndirection.size() * 64 + sizeof(int);
		file.write(reinterpret_cast<char*>(&header), sizeof(SectionHeader));
		uint32 ibuf = (uint32)m_materialIndirection.size();
		file.write(reinterpret_cast<char*>(&ibuf), sizeof(uint32));
		//file.write(reinterpret_cast<char*>(&header), sizeof(SectionHeader));
		char zeroBuf[64] = {0};
		for(uint idx : m_materialIndirection)
		{
			const char* str = m_materials[idx].getName().c_str();
			size_t len = strlen(str);
			file.write(str, len+1);
			file.write(zeroBuf, 63 - len);
		}

		if(_envFile)
			storeEnv(_envFile);
	}

	template<typename T>
	static void loadFileChunk(std::ifstream& _file, uint64 _size, std::vector<T>& _data, Property::Val& _chunkProp, Property::Val _newProp)
	{
		// ASSERT: _size % sizeof(T) == 0
		// Reserve the exact amount of memory
		swap(_data, std::vector<T>(_size / sizeof(T)));
		_file.read(reinterpret_cast<char*>(_data.data()), _size);
		_chunkProp = Property::Val(_chunkProp | _newProp);
	}

	void BinaryModel::makeChunkResident(const ei::IVec3& _chunk)
	{
		int idx = dot(m_dimScale, _chunk);
		// Is it still there?
		if(m_chunkStates[idx] == ChunkState::RELEASE_REQUEST)
			m_chunkStates[idx] = ChunkState::LOADED;
		// Not in memory?
		if(m_chunkStates[idx] != ChunkState::LOADED)
		{
			m_file.clear(); // Clear the error-bits (otherwise seekg fails for eof)
			m_file.seekg(m_chunks[idx].m_address, std::ios_base::beg);
			// Now read the real data
			SectionHeader header;
			while(m_file.read(reinterpret_cast<char*>(&header), sizeof(SectionHeader)) && header.type != CHUNK_SECTION)
			{
				// Should this property be loaded?
				if(m_loadAll || ((m_requestedProps & header.type) != 0) || ((m_optionalProperties & header.type) != 0))
				{
					switch(header.type)
					{
						case CHUNK_META_SECTION: {
							ChunkMetaSection meta;
							m_file.read(reinterpret_cast<char*>(&meta), sizeof(ChunkMetaSection));
							m_chunks[idx].m_boundingBox = meta.boundingBox;
							m_chunks[idx].m_numTrianglesPerLeaf = meta.m_numTrianglesPerLeaf;
							m_chunks[idx].m_numTreeLevels = meta.m_numTreeLevels;
							break; }
						case Property::POSITION: loadFileChunk(m_file, header.size, m_chunks[idx].m_positions, m_chunks[idx].m_properties, Property::POSITION); break;
						case Property::NORMAL: loadFileChunk(m_file, header.size, m_chunks[idx].m_normals, m_chunks[idx].m_properties, Property::NORMAL); break;
						case Property::TANGENT: loadFileChunk(m_file, header.size, m_chunks[idx].m_tangents, m_chunks[idx].m_properties, Property::TANGENT); break;
						case Property::BITANGENT: loadFileChunk(m_file, header.size, m_chunks[idx].m_bitangents, m_chunks[idx].m_properties, Property::BITANGENT); break;
						case Property::QORMAL: loadFileChunk(m_file, header.size, m_chunks[idx].m_qormals, m_chunks[idx].m_properties, Property::QORMAL); break;
						case Property::TEXCOORD0: loadFileChunk(m_file, header.size, m_chunks[idx].m_texCoords0, m_chunks[idx].m_properties, Property::TEXCOORD0); break;
						case Property::TEXCOORD1: loadFileChunk(m_file, header.size, m_chunks[idx].m_texCoords1, m_chunks[idx].m_properties, Property::TEXCOORD1); break;
						case Property::TEXCOORD2: loadFileChunk(m_file, header.size, m_chunks[idx].m_texCoords2, m_chunks[idx].m_properties, Property::TEXCOORD2); break;
						case Property::TEXCOORD3: loadFileChunk(m_file, header.size, m_chunks[idx].m_texCoords3, m_chunks[idx].m_properties, Property::TEXCOORD3); break;
						case Property::COLOR: loadFileChunk(m_file, header.size, m_chunks[idx].m_colors, m_chunks[idx].m_properties, Property::COLOR); break;
						case Property::TRIANGLE_IDX: loadFileChunk(m_file, header.size, m_chunks[idx].m_triangles, m_chunks[idx].m_properties, Property::TRIANGLE_IDX); break;
						case Property::TRIANGLE_MAT: loadFileChunk(m_file, header.size, m_chunks[idx].m_triangleMaterials, m_chunks[idx].m_properties, Property::TRIANGLE_MAT); break;
						case Property::HIERARCHY: loadFileChunk(m_file, header.size, m_chunks[idx].m_hierarchy, m_chunks[idx].m_properties, Property::HIERARCHY); break;
						case HIERARCHY_LEAVES: loadFileChunk(m_file, header.size, m_chunks[idx].m_hierarchyLeaves, m_chunks[idx].m_properties, Property::DONT_CARE); break;
						case Property::AABOX_BVH: loadFileChunk(m_file, header.size, m_chunks[idx].m_aaBoxes, m_chunks[idx].m_properties, Property::AABOX_BVH); break;
						case Property::OBOX_BVH: loadFileChunk(m_file, header.size, m_chunks[idx].m_oBoxes, m_chunks[idx].m_properties, Property::OBOX_BVH); break;
						case Property::NDF_SGGX: loadFileChunk(m_file, header.size, m_chunks[idx].m_nodeNDFs, m_chunks[idx].m_properties, Property::NDF_SGGX); break;
						default: m_file.seekg(header.size, std::ios_base::cur);
					}
				} else m_file.seekg(header.size, std::ios_base::cur);
			}

			if((m_requestedProps & m_chunks[idx].m_properties) != m_requestedProps)
			{
				// Warn here, but continue. Missing properties are filled by defaults.
				std::cerr << "File does not contain the requested properties!\n";
				// Fill in the missing chunk properties
				Property::Val missing = Property::Val(m_requestedProps ^ (m_requestedProps & m_chunks[idx].m_properties));
				for(uint32 i = 1; i != 0; i<<=1)
					if(missing & i)
						m_chunks[idx].addProperty(Property::Val(i));
			}
	
			m_chunkStates[idx] = ChunkState::LOADED;
		}
	}

	void BinaryModel::makeChunkResidentAsync(const ei::IVec3& _chunk)
	{
	}

	bool BinaryModel::isChunkResident(const ei::IVec3& _chunk) const
	{
		return m_chunkStates[dot(m_dimScale, _chunk)] == ChunkState::LOADED;
	}

	void BinaryModel::realeaseChunk(const ei::IVec3& _chunk)
	{
		int idx = dot(m_dimScale, _chunk);
		// Make sure the bounding box invariant holds (all unloaded chunks
		// are proper represented).
		m_boundingBox = ei::Box(m_boundingBox, m_chunks[idx].m_boundingBox);
		m_chunkStates[idx] = ChunkState::RELEASE_REQUEST;
	}


	template<typename T>
	static void storeFileChunk(std::ofstream& _file, uint32 _type, const std::vector<T>& _data)
	{
		SectionHeader header;
		header.type = _type;
		header.size = _data.size() * sizeof(T);
		_file.write(reinterpret_cast<const char*>(&header), sizeof(SectionHeader));
		_file.write(reinterpret_cast<const char*>(_data.data()), header.size);
	}

	void BinaryModel::storeChunk(const char* _bimFile, const ei::IVec3& _chunkPos)
	{
		if(!isChunkResident(_chunkPos)) {std::cerr << "Chunk is not resident and cannot be stored!\n"; return;}
		int idx = dot(m_dimScale, _chunkPos);
		std::ofstream file(_bimFile, std::ios_base::binary | std::ios_base::app);
		if(m_file.bad()) {std::cerr << "Cannot open file for writing a chunk!\n"; return;}
	
		SectionHeader header;
		header.type = CHUNK_SECTION;
		header.size = m_chunks[idx].m_positions.size() * sizeof(ei::Vec3) +
					  m_chunks[idx].m_triangles.size() * sizeof(ei::UVec3) +
					  sizeof(SectionHeader) * 2 + sizeof(ChunkMetaSection);
		if(m_chunks[idx].m_properties & Property::NORMAL)
			header.size += m_chunks[idx].m_normals.size() * sizeof(ei::Vec3) + sizeof(SectionHeader);
		if(m_chunks[idx].m_properties & Property::TANGENT)
			header.size += m_chunks[idx].m_tangents.size() * sizeof(ei::Vec3) + sizeof(SectionHeader);
		if(m_chunks[idx].m_properties & Property::BITANGENT)
			header.size += m_chunks[idx].m_bitangents.size() * sizeof(ei::Vec3) + sizeof(SectionHeader);
		if(m_chunks[idx].m_properties & Property::QORMAL)
			header.size += m_chunks[idx].m_qormals.size() * sizeof(ei::Quaternion) + sizeof(SectionHeader);
		if(m_chunks[idx].m_properties & Property::TEXCOORD0)
			header.size += m_chunks[idx].m_texCoords0.size() * sizeof(ei::Vec2) + sizeof(SectionHeader);
		if(m_chunks[idx].m_properties & Property::TEXCOORD1)
			header.size += m_chunks[idx].m_texCoords1.size() * sizeof(ei::Vec2) + sizeof(SectionHeader);
		if(m_chunks[idx].m_properties & Property::TEXCOORD2)
			header.size += m_chunks[idx].m_texCoords2.size() * sizeof(ei::Vec2) + sizeof(SectionHeader);
		if(m_chunks[idx].m_properties & Property::TEXCOORD3)
			header.size += m_chunks[idx].m_texCoords3.size() * sizeof(ei::Vec2) + sizeof(SectionHeader);
		if(m_chunks[idx].m_properties & Property::COLOR)
			header.size += m_chunks[idx].m_colors.size() * sizeof(uint32) + sizeof(SectionHeader);
		if(m_chunks[idx].m_properties & Property::TRIANGLE_MAT)
			header.size += m_chunks[idx].m_triangleMaterials.size() * sizeof(uint32) + sizeof(SectionHeader);
		if(m_chunks[idx].m_properties & Property::HIERARCHY)
			header.size += m_chunks[idx].m_hierarchy.size() * sizeof(Node) + sizeof(SectionHeader) * 2
							+ m_chunks[idx].m_hierarchyLeaves.size() * sizeof(ei::UVec4);
		if(m_chunks[idx].m_properties & Property::AABOX_BVH)
			header.size += m_chunks[idx].m_aaBoxes.size() * sizeof(ei::Box) + sizeof(SectionHeader);
		if(m_chunks[idx].m_properties & Property::OBOX_BVH)
			header.size += m_chunks[idx].m_oBoxes.size() * sizeof(ei::OBox) + sizeof(SectionHeader);
		if(m_chunks[idx].m_properties & Property::NDF_SGGX)
			header.size += m_chunks[idx].m_nodeNDFs.size() * sizeof(SGGX) + sizeof(SectionHeader);
		file.write(reinterpret_cast<const char*>(&header), sizeof(SectionHeader));

		header.type = CHUNK_META_SECTION;
		header.size = sizeof(ChunkMetaSection);
		ChunkMetaSection meta;
		meta.boundingBox = m_chunks[idx].m_boundingBox;
		meta.m_numTrianglesPerLeaf = m_chunks[idx].m_numTrianglesPerLeaf;
		meta.m_numTreeLevels = m_chunks[idx].m_numTreeLevels;
		file.write(reinterpret_cast<const char*>(&header), sizeof(SectionHeader));
		file.write(reinterpret_cast<const char*>(&meta), sizeof(ChunkMetaSection));
	
		// Vertex stuff
		storeFileChunk(file, Property::POSITION, m_chunks[idx].m_positions);
		if(m_chunks[idx].m_properties & Property::NORMAL)
			storeFileChunk(file, Property::NORMAL, m_chunks[idx].m_normals);
		if(m_chunks[idx].m_properties & Property::TANGENT)
			storeFileChunk(file, Property::TANGENT, m_chunks[idx].m_tangents);
		if(m_chunks[idx].m_properties & Property::BITANGENT)
			storeFileChunk(file, Property::BITANGENT, m_chunks[idx].m_bitangents);
		if(m_chunks[idx].m_properties & Property::QORMAL)
			storeFileChunk(file, Property::QORMAL, m_chunks[idx].m_qormals);
		if(m_chunks[idx].m_properties & Property::TEXCOORD0)
			storeFileChunk(file, Property::TEXCOORD0, m_chunks[idx].m_texCoords0);
		if(m_chunks[idx].m_properties & Property::TEXCOORD1)
			storeFileChunk(file, Property::TEXCOORD1, m_chunks[idx].m_texCoords1);
		if(m_chunks[idx].m_properties & Property::TEXCOORD2)
			storeFileChunk(file, Property::TEXCOORD2, m_chunks[idx].m_texCoords2);
		if(m_chunks[idx].m_properties & Property::TEXCOORD3)
			storeFileChunk(file, Property::TEXCOORD3, m_chunks[idx].m_texCoords3);
		if(m_chunks[idx].m_properties & Property::COLOR)
			storeFileChunk(file, Property::COLOR, m_chunks[idx].m_colors);

		// Triangle stuff
		storeFileChunk(file, Property::TRIANGLE_IDX, m_chunks[idx].m_triangles);
		if(m_chunks[idx].m_properties & Property::TRIANGLE_MAT)
			storeFileChunk(file, Property::TRIANGLE_MAT, m_chunks[idx].m_triangleMaterials);

		// Hierarchy stuff
		if(m_chunks[idx].m_properties & Property::HIERARCHY)
		{
			storeFileChunk(file, Property::HIERARCHY, m_chunks[idx].m_hierarchy);
			storeFileChunk(file, HIERARCHY_LEAVES, m_chunks[idx].m_hierarchyLeaves);
		}
		if(m_chunks[idx].m_properties & Property::AABOX_BVH)
			storeFileChunk(file, Property::AABOX_BVH, m_chunks[idx].m_aaBoxes);
		if(m_chunks[idx].m_properties & Property::OBOX_BVH)
			storeFileChunk(file, Property::OBOX_BVH, m_chunks[idx].m_oBoxes);
		if(m_chunks[idx].m_properties & Property::NDF_SGGX)
			storeFileChunk(file, Property::NDF_SGGX, m_chunks[idx].m_nodeNDFs);
	}

	bool BinaryModel::loadEnv(const char* _envFile)
	{
		Json json;
		Json::Value currentVal;
		if(!json.open(_envFile, currentVal)) {
			std::cerr << "Opening environment JSON failed!\n";
			return false;
		}

		// Iterate through all materials
		if(json.child(currentVal, currentVal))
		if(strcmp(currentVal.getName(), "materials") == 0)
		{
			if(json.child(currentVal, currentVal))
			do {
				Material mat;
				mat.m_name = currentVal.getName();
				Json::Value matProp;
				if(json.child(currentVal, matProp))
				// A material contains a list of strings or float (arrays).
				do {
					if(matProp.getType() == Json::ValueType::STRING)
						mat.m_textureNames.emplace(matProp.getName(), matProp.getString());
					else if(matProp.getType() == Json::ValueType::ARRAY)
					{
						// Assume a float vector
						ei::Vec4 value(0.0f);
						Json::Value v; json.child(matProp, v);
						value[0] = v.getFloat();
						if(json.next(v, v)) value[1] = v.getFloat();
						if(json.next(v, v)) value[2] = v.getFloat();
						if(json.next(v, v)) value[3] = v.getFloat();
						mat.m_values.emplace(matProp.getName(), value);
					} else if(matProp.getType() == Json::ValueType::FLOAT)
						mat.m_values.emplace(matProp.getName(), ei::Vec4(matProp.getFloat(), 0.0f, 0.0f, 0.0f));
					else if(matProp.getType() == Json::ValueType::INT)
						mat.m_values.emplace(matProp.getName(), ei::Vec4((float)matProp.getInt(), 0.0f, 0.0f, 0.0f));
				} while(json.next(matProp, matProp));
				m_materials.push_back(mat);
			} while(json.next(currentVal, currentVal));
		}
		return true;
	}

	void BinaryModel::storeEnv(const char* _envFile)
	{
		JsonWriter json;
		if(!json.open(_envFile)) {
			std::cerr << "Opening environment JSON failed!\n";
			return;
		}
		json.beginObject();
		json.valuePreamble("materials");
		json.beginObject();
		for(auto& mat : m_materials)
		{
			json.valuePreamble(mat.getName().c_str());
			json.beginObject();
			for(auto& tex : mat.m_textureNames)
			{
				json.valuePreamble(tex.first.c_str());
				json.value(tex.second.c_str());
			}
			for(auto& val : mat.m_values)
			{
				json.valuePreamble(val.first.c_str());
				json.value(val.second.m_data, 4);
			}
			json.endObject();
		}
		json.endObject();
		json.endObject();
	}
}