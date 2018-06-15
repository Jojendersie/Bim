#include "bim/bim.hpp"
#include "../deps/json/json.hpp"
#include "../deps/miniz.c"
#include "../deps/EnumConverter.h"
#include "bim/log.hpp"
#include <fstream>
#include <memory>

static const char* propertyString(bim::Property::Val _prop)
{
	switch(_prop)
	{
		case bim::Property::POSITION: return "POSITION";
		case bim::Property::NORMAL: return "NORMAL";
		case bim::Property::TANGENT: return "TANGENT";
		case bim::Property::BITANGENT: return "BITANGENT";
		case bim::Property::QORMAL: return "QORMAL";
		case bim::Property::TEXCOORD0: return "TEXCOORD1";
		case bim::Property::TEXCOORD1: return "POSITION";
		case bim::Property::TEXCOORD2: return "TEXCOORD2";
		case bim::Property::TEXCOORD3: return "TEXCOORD3";
		case bim::Property::COLOR: return "COLOR";
		case bim::Property::TRIANGLE_IDX: return "TRIANGLE_IDX";
		case bim::Property::TRIANGLE_MAT: return "TRIANGLE_MAT";
		case bim::Property::AABOX_BVH: return "AABOX_BVH";
		case bim::Property::OBOX_BVH: return "OBOX_BVH";
		case bim::Property::SPHERE_BVH: return "SPHERE_BVH";
		case bim::Property::HIERARCHY: return "HIERARCHY";
		case bim::Property::NDF_SGGX: return "NDF_SGGX";
		default: return "UNKNOWN";
	}
}

namespace bim {

	struct SectionHeader
	{
		uint32 type;	// BinaryModel::Property::Val + extra values
		uint64 size;	// Block size of following data
		uint64 uncompressedSize;	// Size of the data after decompression (INFLATE) or 0 if data is not compressed.
	};

	// Non powers of 2 are free to use (others are reserved for Property::...)
	const int META_SECTION = 0x0;
	const int CHUNK_SECTION = 0x3;
	const int MATERIAL_REFERENCE = 0x5;
	const int HIERARCHY_PARENTS = 0x08000001;
	const int HIERARCHY_LEAVES = 0x08000002;
	const int CHUNK_META_SECTION = 0x6;

	struct MetaSection
	{
		ei::IVec3 numChunks;	// Number of stored chunks
		ei::Box boundingBox;	// Entire scene bounding box
	};

	struct ChunkMetaSection
	{
		ei::Box boundingBox;
		uint numTreeLevels;
	};

	std::string pathOf(const char* _file)
	{
		const char* begin = _file;
		const char* end = _file;
		while(*_file != 0)
		{
			if(*_file == '\\' || *_file == '/')
				end = _file + 1;
			++_file;
		}
		return std::string(begin, end);
	}

	bool BinaryModel::load(const char* _envFile, Property::Val _requiredProperties, Property::Val _optionalProperties, bool _loadAll)
	{
		std::string bimFile = loadEnv(_envFile, false);
		if(bimFile.empty()) {
			sendMessage(MessageType::ERROR, "The Environment-File did not contain a binary file reference!");
			return false;
		}

		// The bimFile is a relative path -> append the path from the envFile.
		bimFile = pathOf(_envFile) + bimFile;

		m_file.open(bimFile, std::ios_base::binary);
		if(m_file.fail()) {
			sendMessage(MessageType::ERROR, "Cannot open scene file!");
			return false;
		}
		SectionHeader header;
	
		// Analyse file:
		//  * Is the requested information available?
		//  * Get the jump addresses of each chunk.
		m_file.read(reinterpret_cast<char*>(&header), sizeof(SectionHeader));
		if((header.type != META_SECTION) || (header.size != sizeof(MetaSection))) {
			sendMessage(MessageType::ERROR, "Invalid file. Meta-section not found!");
			return false;
		}

		MetaSection meta;
		m_file.read(reinterpret_cast<char*>(&meta), sizeof(MetaSection));
		m_loadAll = _loadAll;
		// Make sure at least positions and triangles are available
		m_requestedProps = Property::Val(_requiredProperties | Property::POSITION | Property::TRIANGLE_IDX);
		// Use the accelerator from environment file or load a default if hierarchy is required.
		if(m_accelerator != Property::DONT_CARE)
			m_requestedProps = Property::Val(m_requestedProps | m_accelerator);
		else if((_requiredProperties & Property::HIERARCHY)
			&& !(_requiredProperties & Property::AABOX_BVH)
			&& !(_requiredProperties & Property::OBOX_BVH))
			m_requestedProps = Property::Val(m_requestedProps | Property::AABOX_BVH);
		m_optionalProperties = _optionalProperties;
		m_numChunks = meta.numChunks;
		m_dimScale = ei::IVec3(1, m_numChunks.x, m_numChunks.x * m_numChunks.y);
		m_boundingBox = meta.boundingBox;
	
		m_chunks.clear();
		m_chunkStates.clear();
		Chunk emptyChunk(this);
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
					m_materialIndirection.push_back(buf);
				}
			} else
				m_file.seekg(header.size, std::ios_base::cur);
		}

		// Validation
		if(m_chunks.size() != prod(m_numChunks))
		{
			std::string msg = "Invalid number of chunks. Expected " + std::to_string(prod(m_numChunks)) + " found " + std::to_string(m_chunks.size());
			sendMessage(MessageType::ERROR, msg.c_str());
			return false;
		}

		// If there where no material references stored build a dummy map
		// to all existing materials.
		if(m_materialIndirection.empty())
		{
			for(auto it : m_materials)
				m_materialIndirection.push_back(it.second.getName());
		}

		return true;
	}

	void BinaryModel::loadEnvironmentFile(const char * _envFile)
	{
		loadEnv(_envFile, true);
	}

	void BinaryModel::storeBinaryHeader(const char * _bimFile)
	{
		std::ofstream file(_bimFile, std::ios_base::binary | std::ios_base::out);
		if(m_file.bad()) {sendMessage(MessageType::ERROR, "Cannot open file for writing!"); return;}
		SectionHeader header;
		header.uncompressedSize = 0;
	
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
		for(auto& str : m_materialIndirection)
		{
			file.write(str.c_str(), str.length()+1);
			file.write(zeroBuf, 63 - str.length());
		}
	}

	template<typename T>
	static void loadFileChunk(std::ifstream& _file, const SectionHeader& _header, std::vector<T>& _data, Property::Val& _chunkProp, Property::Val _newProp)
	{
		if(_header.uncompressedSize)
		{
			if(_header.uncompressedSize % sizeof(T) != 0) {
				sendMessage(MessageType::ERROR, "Error while loading a chunk: data size is incompatible with data type.");
				return;
			}
			// Reserve the exact amount of memory
			swap(_data, std::vector<T>(_header.uncompressedSize / sizeof(T)));
			std::unique_ptr<byte[]> compressedBuffer(new byte[_header.size]);
			_file.read(reinterpret_cast<char*>(compressedBuffer.get()), _header.size);

			mz_ulong size = (mz_ulong)_header.uncompressedSize;
			int r = uncompress(reinterpret_cast<byte*>(_data.data()), &size, compressedBuffer.get(), (mz_ulong)_header.size);
			if(r != Z_OK || _header.uncompressedSize != size) {
				sendMessage(MessageType::ERROR, "Error in chunk decompression.");
				// TODO : more information
				return;
			}
		} else {
			if(_header.size % sizeof(T) != 0) {
				sendMessage(MessageType::ERROR, "Error while loading a chunk: data size is incompatible with data type.");
				return;
			}
			// Reserve the exact amount of memory
			swap(_data, std::vector<T>(_header.size / sizeof(T)));
			_file.read(reinterpret_cast<char*>(_data.data()), _header.size);
		}
		_chunkProp = Property::Val(_chunkProp | _newProp);
	}

	Chunk * BinaryModel::getChunk(const ei::IVec3 & _chunkPos)
	{
		int chunkIndex = dot(_chunkPos, m_dimScale);
		if(m_chunkStates[chunkIndex] == ChunkState::LOADED)
			return &m_chunks[chunkIndex];
		sendMessage(MessageType::ERROR, "Chunk is not resident. getChunk() is invalid in this state.");
		return nullptr;
	}

	void BinaryModel::makeChunkResident(const ei::IVec3& _chunk)
	{
		int idx = dot(m_dimScale, _chunk);
		// Is it still there?
		if(m_chunkStates[idx] == ChunkState::RELEASE_REQUEST)
			m_chunkStates[idx] = ChunkState::LOADED;
		// New chunk?
		if(m_chunkStates[idx] == ChunkState::EMPTY && m_chunks[idx].m_address == 0)
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
							m_chunks[idx].m_numTreeLevels = meta.numTreeLevels;
							break; }
						case Property::POSITION: loadFileChunk(m_file, header, m_chunks[idx].m_positions, m_chunks[idx].m_properties, Property::POSITION); break;
						case Property::NORMAL: loadFileChunk(m_file, header, m_chunks[idx].m_normals, m_chunks[idx].m_properties, Property::NORMAL); break;
						case Property::TANGENT: loadFileChunk(m_file, header, m_chunks[idx].m_tangents, m_chunks[idx].m_properties, Property::TANGENT); break;
						case Property::BITANGENT: loadFileChunk(m_file, header, m_chunks[idx].m_bitangents, m_chunks[idx].m_properties, Property::BITANGENT); break;
						case Property::QORMAL: loadFileChunk(m_file, header, m_chunks[idx].m_qormals, m_chunks[idx].m_properties, Property::QORMAL); break;
						case Property::TEXCOORD0: loadFileChunk(m_file, header, m_chunks[idx].m_texCoords0, m_chunks[idx].m_properties, Property::TEXCOORD0); break;
						case Property::TEXCOORD1: loadFileChunk(m_file, header, m_chunks[idx].m_texCoords1, m_chunks[idx].m_properties, Property::TEXCOORD1); break;
						case Property::TEXCOORD2: loadFileChunk(m_file, header, m_chunks[idx].m_texCoords2, m_chunks[idx].m_properties, Property::TEXCOORD2); break;
						case Property::TEXCOORD3: loadFileChunk(m_file, header, m_chunks[idx].m_texCoords3, m_chunks[idx].m_properties, Property::TEXCOORD3); break;
						case Property::COLOR: loadFileChunk(m_file, header, m_chunks[idx].m_colors, m_chunks[idx].m_properties, Property::COLOR); break;
						case Property::TRIANGLE_IDX: loadFileChunk(m_file, header, m_chunks[idx].m_triangles, m_chunks[idx].m_properties, Property::TRIANGLE_IDX); break;
						case Property::TRIANGLE_MAT: loadFileChunk(m_file, header, m_chunks[idx].m_triangleMaterials, m_chunks[idx].m_properties, Property::TRIANGLE_MAT); break;
						case Property::HIERARCHY: loadFileChunk(m_file, header, m_chunks[idx].m_hierarchy, m_chunks[idx].m_properties, Property::HIERARCHY); break;
						case HIERARCHY_PARENTS: loadFileChunk(m_file, header, m_chunks[idx].m_hierarchyParents, m_chunks[idx].m_properties, Property::DONT_CARE); break;
						case HIERARCHY_LEAVES: loadFileChunk(m_file, header, m_chunks[idx].m_hierarchyLeaves, m_chunks[idx].m_properties, Property::DONT_CARE); break;
						case Property::AABOX_BVH: loadFileChunk(m_file, header, m_chunks[idx].m_aaBoxes, m_chunks[idx].m_properties, Property::AABOX_BVH); break;
						case Property::OBOX_BVH: loadFileChunk(m_file, header, m_chunks[idx].m_oBoxes, m_chunks[idx].m_properties, Property::OBOX_BVH); break;
						case Property::NDF_SGGX: loadFileChunk(m_file, header, m_chunks[idx].m_nodeNDFs, m_chunks[idx].m_properties, Property::NDF_SGGX); break;
						default: m_file.seekg(header.size, std::ios_base::cur);
					}
				} else m_file.seekg(header.size, std::ios_base::cur);
			}

			if((m_requestedProps & m_chunks[idx].m_properties) != m_requestedProps)
			{
				// Warn here, but continue. Missing properties are filled by defaults.
				sendMessage(MessageType::WARNING, "File does not contain the requested properties! Missing:");
				// Fill in the missing chunk properties
				Property::Val missing = Property::Val(m_requestedProps ^ (m_requestedProps & m_chunks[idx].m_properties));
				for(uint32 i = 1; i != 0; i<<=1)
					if(missing & i)
					{
						m_chunks[idx].addProperty(Property::Val(i));
						sendMessage(MessageType::WARNING, "    ", propertyString(Property::Val(i)));
					}
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

	void BinaryModel::deleteChunk(const ei::IVec3 & _chunkPos)
	{
		int idx = dot(m_dimScale, _chunkPos);
		m_chunkStates[idx] = ChunkState::EMPTY;
		// Create an empty chunk which preserves some of the properties
		Chunk emptyChunk(this);
		emptyChunk.m_properties = m_chunks[idx].m_properties;
		emptyChunk.m_address = m_chunks[idx].m_address;
		emptyChunk.m_boundingBox = m_chunks[idx].m_boundingBox;
		// Exchange and let the destructor destroy the data.
		m_chunks[idx] = emptyChunk;
	}


	template<typename T>
	static void storeFileChunk(std::ofstream& _file, uint32 _type, const std::vector<T>& _data)
	{
		SectionHeader header;
		header.type = _type;
		header.uncompressedSize = _data.size() * sizeof(T);

		// Compress data
		mz_ulong compressedSize = compressBound((mz_ulong)header.uncompressedSize);
		std::unique_ptr<byte[]> compressedBuffer = std::make_unique<byte[]>(compressedSize);
		int r = compress2(compressedBuffer.get(), &compressedSize, reinterpret_cast<const byte*>(_data.data()), (mz_ulong)header.uncompressedSize, 9);
		if(r != Z_OK) {
			sendMessage(MessageType::ERROR, "Failed to compress a data chunk!");
			return;
		}
		header.size = compressedSize;

		_file.write(reinterpret_cast<const char*>(&header), sizeof(SectionHeader));
		//_file.write(reinterpret_cast<const char*>(_data.data()), header.size);
		_file.write(reinterpret_cast<const char*>(compressedBuffer.get()), header.size);
	}

	void BinaryModel::storeChunk(const char* _bimFile, const ei::IVec3& _chunkPos)
	{
		if(!isChunkResident(_chunkPos)) {sendMessage(MessageType::ERROR, "Chunk is not resident and cannot be stored!"); return;}
		int idx = dot(m_dimScale, _chunkPos);
		std::ofstream file(_bimFile, std::ios_base::binary | std::ios_base::in | std::ios_base::out);
		if(m_file.bad()) {sendMessage(MessageType::ERROR, "Cannot open file for writing a chunk!"); return;}
		// Append at the end..
		file.seekp(0, std::ios_base::end);
	
		SectionHeader header;
		header.type = CHUNK_SECTION;
		header.size = -1;
		header.uncompressedSize = -1;
		// Currently we don't know the final size due to compression.
		// Store a dummy header first and then go back later.
		uint64 headerPos = file.tellp();
		file.write(reinterpret_cast<const char*>(&header), sizeof(SectionHeader));

		header.type = CHUNK_META_SECTION;
		header.size = sizeof(ChunkMetaSection);
		header.uncompressedSize = 0;
		ChunkMetaSection meta;
		meta.boundingBox = m_chunks[idx].m_boundingBox;
		meta.numTreeLevels = m_chunks[idx].m_numTreeLevels;
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
			storeFileChunk(file, HIERARCHY_PARENTS, m_chunks[idx].m_hierarchyParents);
			storeFileChunk(file, HIERARCHY_LEAVES, m_chunks[idx].m_hierarchyLeaves);
		}
		if(m_chunks[idx].m_properties & Property::AABOX_BVH)
			storeFileChunk(file, Property::AABOX_BVH, m_chunks[idx].m_aaBoxes);
		if(m_chunks[idx].m_properties & Property::OBOX_BVH)
			storeFileChunk(file, Property::OBOX_BVH, m_chunks[idx].m_oBoxes);
		if(m_chunks[idx].m_properties & Property::NDF_SGGX)
			storeFileChunk(file, Property::NDF_SGGX, m_chunks[idx].m_nodeNDFs);

		// Query the correct size and rewrite the header.
		header.type = CHUNK_SECTION;
		header.size = uint64(file.tellp()) - headerPos - sizeof(SectionHeader);
		header.uncompressedSize = 0;
		file.seekp(headerPos);
		file.write(reinterpret_cast<const char*>(&header), sizeof(SectionHeader));
	}

	std::string BinaryModel::loadEnv(const char* _envFile, bool _ignoreBinary)
	{
		std::string binarySceneFile;
		std::ifstream envFile(_envFile);
		if(!envFile) {
			sendMessage(MessageType::ERROR, "Opening environment JSON failed!");
			return move(binarySceneFile);
		}
		Json jsonRoot;
		try {
			envFile >> jsonRoot;

			// Make sure that there is always a default scenario at the
			// first place.
			addScenario("default");

			auto it = jsonRoot.find("materials");
			if(it != jsonRoot.end())
			{
				for(Json::iterator itm = it->begin(); itm != it->end(); ++itm)
					loadMaterial(itm.value(), itm.key());
			} else sendMessage(MessageType::ERROR, "Cannot find 'materials' section in the scene file!");

			if((it = jsonRoot.find("scene")) != jsonRoot.end())
				binarySceneFile = it.value();
			else sendMessage(MessageType::ERROR, "Cannot find 'scene' binary file name!");

			if((it = jsonRoot.find("accelerator")) != jsonRoot.end())
			{
				const std::string& str = *it;
				if(strcmp(str.c_str(), "aabox") == 0) m_accelerator = Property::AABOX_BVH;
				else if(strcmp(str.c_str(), "obox") == 0) m_accelerator = Property::OBOX_BVH;
				else sendMessage(MessageType::WARNING, "Unknown accelerator in environment file. Only 'aabox' and 'obox' are valid.");
			}

			if((it = jsonRoot.find("lights")) != jsonRoot.end())
			{
				for(Json::iterator itl = it->begin(); itl != it->end(); ++itl)
					loadLight(itl.value(), itl.key());
			} // No lights is OK - scene may contain emissive surfaces.

			if((it = jsonRoot.find("cameras")) != jsonRoot.end())
				for(Json::iterator itc = it->begin(); itc != it->end(); ++itc)
					loadCamera(itc.value(), itc.key());
			else sendMessage(MessageType::ERROR, "Cannot find 'cameras' section in the scene file!");
		} catch(const std::exception& _e) {
			sendMessage(MessageType::ERROR, _e.what());
		}

		return move(binarySceneFile);
	}

	void BinaryModel::loadMaterial(const Json& _node, const std::string& _name)
	{
		Material mat;
		mat.m_name = _name;
		// A material contains a list of strings or float (arrays).
		//for(Json::iterator it : _node)
		for(Json::const_iterator it = _node.begin(); it != _node.end(); ++it)
		{
			if(it->is_string()) {
				if(strcmp(it.key().c_str(), "type") == 0)
					mat.setType(*it);
				else
					mat.m_textureNames.emplace(it.key(), it.value());
			} else if(it->is_array())
			{
				// Assume a float vector
				Material::MultiValue value{ei::Vec4(0.0f), 0};
				for(int i = 0; i < ei::min(4, int(it->size())); ++i)
				{
					value.values[i] = it.value()[i];
					value.numComponents++;
				}
				mat.m_values.emplace(it.key(), value);
			} else
				mat.m_values.emplace(it.key(), Material::MultiValue{ei::Vec4(*it, 0.0f, 0.0f, 0.0f), 1});
		}
		m_materials.emplace(mat.getName(), mat);
	}

	static ei::Vec3 readVec3(const Json& _node)
	{
		ei::Vec3 value(0.0f);
		// Fallback to zero if something goes wrong
		try { value.x = _node[0]; } catch(...) {}
		try { value.y = _node[1]; } catch(...) {}
		try { value.z = _node[2]; } catch(...) {}
		return value;
	}

	void BinaryModel::loadLight(const Json& _node, const std::string& _name)
	{
		// Default values for all possible light properties (some are not
		// used dependent on the final type).
		Light::Type type = Light::Type::NUM_TYPES;
		ei::Vec3 position(0.0f);
		ei::Vec3 intensity(10000.0f); // Also used as radiance or intensity scale
		ei::Vec3 normal(0.0f, 1.0f, 0.0f); // Also used as light direction
		float falloff = 10.0f;
		float halfAngle = 0.7f;
		float turbidity = 2.0f;
		bool aerialPerspective = false;
		bool hasExplicitScenarios = false;
		std::string map;
		std::vector<std::string> scenarios;

		auto it = _node.find("type");
		if(it != _node.end())
		{
			type = Light::TypeFromString(*it);
		} else sendMessage(MessageType::ERROR, "No type given for light source ", _name);

		if((it = _node.find("position")) != _node.end()) position = readVec3(*it);
		if((it = _node.find("intensity")) != _node.end()) intensity = readVec3(*it);
		if((it = _node.find("normal")) != _node.end()) normal = readVec3(*it);
		if((it = _node.find("direction")) != _node.end()) normal = readVec3(*it);
		if((it = _node.find("irradiance")) != _node.end()) intensity = readVec3(*it);
		if((it = _node.find("peakIntensity")) != _node.end()) intensity = readVec3(*it);
		if((it = _node.find("falloff")) != _node.end()) falloff = *it;
		if((it = _node.find("halfAngle")) != _node.end()) halfAngle =  *it;
		if((it = _node.find("sunDirection")) != _node.end()) normal = readVec3(*it);
		if((it = _node.find("turbidity")) != _node.end()) turbidity = *it;
		if((it = _node.find("aerialPerspective")) != _node.end()) aerialPerspective = *it;
		if((it = _node.find("intensityMap")) != _node.end()) map = *it;
		if((it = _node.find("intensityScale")) != _node.end()) intensity = readVec3(*it);
		if((it = _node.find("radianceMap")) != _node.end()) map = *it;
		if((it = _node.find("scenario")) != _node.end())
		{
			hasExplicitScenarios = true;
			for(const auto& child : *it)
				scenarios.push_back(child);
		}

		normal = normalize(normal);

		switch(type)
		{
		case Light::Type::POINT:
			m_lights.push_back(std::make_shared<PointLight>(position, intensity, _name));
			break;
		case Light::Type::LAMBERT:
			m_lights.push_back(std::make_shared<LambertLight>(position, normal, intensity, _name));
			break;
		case Light::Type::DIRECTIONAL:
			m_lights.push_back(std::make_shared<DirectionalLight>(normal, intensity, _name));
			break;
		case Light::Type::SPOT:
			m_lights.push_back(std::make_shared<SpotLight>(position, normal, intensity, falloff, halfAngle, _name));
			break;
		case Light::Type::SKY:
			m_lights.push_back(std::make_shared<SkyLight>(normal, turbidity, aerialPerspective, _name));
			break;
		case Light::Type::GONIOMETRIC:
			m_lights.push_back(std::make_shared<GoniometricLight>(position, intensity, map, _name));
			break;
		case Light::Type::ENVIRONMENT:
			m_lights.push_back(std::make_shared<EnvironmentLight>(map, _name));
			break;
		default:
			sendMessage(MessageType::ERROR, "Light ", _name, " does not have a type!");
			return;
		}

		// Always add to the default scenario if nothing was specified
		if(!hasExplicitScenarios)
			scenarios.push_back("default");

		for(const auto& sname : scenarios)
		{
			Scenario* scenario = getScenario(sname);
			if(!scenario)
				scenario = addScenario(sname);
			scenario->addLight(m_lights.back());
		}
	}

	void BinaryModel::loadCamera(const Json& _node, const std::string& _name)
	{
		// Default values
		Camera::Type type = Camera::Type::NUM_TYPES;
		ei::Vec3 position(0.0f);
		ei::Vec3 lookAt(0.0f, 0.0f, 1.0f);
		ei::Vec3 up(0.0f, 1.0f, 0.0f);
		float fieldOfView = 90.0f;
		float left = -1.0f;
		float right = 1.0f;
		float bottom = -1.0f;
		float top = 1.0f;
		float near = 0.0f;
		float far = 1e30f;
		float focalLength = 20.0f;
		float focusDistance = 1.0f;
		float sensorSize = 24.0f;
		float aperture = 1.0f;
		float velocity = 1.0f;
		bool hasExplicitScenarios = false;
		std::vector<std::string> scenarios;

		auto it = _node.find("type");
		if(it != _node.end())
		{
			type = Camera::TypeFromString(*it);
		} else sendMessage(MessageType::ERROR, "No type given for camera ", _name);

		if((it = _node.find("position")) != _node.end()) position = readVec3(*it);
		if((it = _node.find("lookAt")) != _node.end()) lookAt = readVec3(*it);
		if((it = _node.find("viewDir")) != _node.end()) lookAt = position + readVec3(*it);
		if((it = _node.find("up")) != _node.end()) up = readVec3(*it);
		if((it = _node.find("fov")) != _node.end()) fieldOfView = *it;
		if((it = _node.find("left")) != _node.end()) left = *it;
		if((it = _node.find("right")) != _node.end()) right = *it;
		if((it = _node.find("bottom")) != _node.end()) bottom = *it;
		if((it = _node.find("top")) != _node.end()) top = *it;
		if((it = _node.find("near")) != _node.end()) near = *it;
		if((it = _node.find("far")) != _node.end()) far = *it;
		if((it = _node.find("focalLength")) != _node.end()) focalLength = *it;
		if((it = _node.find("focusDistance")) != _node.end()) focusDistance = *it;
		if((it = _node.find("sensorSize")) != _node.end()) sensorSize = *it;
		if((it = _node.find("aperture")) != _node.end()) aperture = *it;
		if((it = _node.find("velocity")) != _node.end()) velocity = *it;
		if((it = _node.find("scenario")) != _node.end())
		{
			hasExplicitScenarios = true;
			for(const auto& child : *it)
				scenarios.push_back(child);
		}

		fieldOfView *= ei::PI / 180.0f;

		switch(type)
		{
		case Camera::Type::PERSPECTIVE:
			m_cameras.push_back(std::make_shared<PerspectiveCamera>(position, lookAt, up, fieldOfView, _name));
			break;
		case Camera::Type::ORTHOGRAPHIC:
			m_cameras.push_back(std::make_shared<OrthographicCamera>(position, lookAt, up, left, right, bottom, top, near, far, _name));
			break;
		case Camera::Type::FOCUS:
			m_cameras.push_back(std::make_shared<FocusCamera>(position, lookAt, up, focalLength, focusDistance, sensorSize, aperture, _name));
			break;
		default:
			sendMessage(MessageType::ERROR, "Camera ", _name, " does not have a type!");
			return;
		}
		m_cameras.back()->velocity = velocity;

		// Always add to the default scenario if nothing was specified
		if(!hasExplicitScenarios)
			scenarios.push_back("default");

		for(auto scenarioName : scenarios)
		{
			Scenario* scenario = getScenario(scenarioName);
			if(!scenario)
				scenario = addScenario(scenarioName);
			scenario->setCamera(m_cameras.back());
		}
	}

	// Extract the relative path of _file with respect to base
	static std::string makeRelative(const char* _base, const char* _file)
	{
		std::string relPath;
		const char* baseDirBegin = _base;
		const char* fileDirBegin = _file;
		bool diverged = false;
		while(*_base != 0 && *_file != 0)
		{
			if(*_base != *_file || diverged)
			{
				diverged = true;
				if(*_base == '\\' || *_base == '/') relPath += "../";
			} else {
				if(*_base == '\\' || *_base == '/') {
					baseDirBegin = _base;
					fileDirBegin = _file;
				}
			}
			++_base; ++_file;
		}
		if(*fileDirBegin == '\\' || *fileDirBegin == '/')
			relPath += fileDirBegin + 1;
		else
			relPath += fileDirBegin;
		return move(relPath);
	}

	void BinaryModel::storeEnvironmentFile(const char* _envFile, const char* _bimFile)
	{
		std::ofstream envFile(_envFile);
		if(!envFile) {
			sendMessage(MessageType::ERROR, "Opening environment JSON failed!");
			return;
		}

		Json json;

		json["scene"] = makeRelative(_envFile, _bimFile);

		if(m_accelerator != Property::DONT_CARE)
		{
			if(m_accelerator == Property::AABOX_BVH)
				json["accelerator"] = "aabox";
			else if(m_accelerator == Property::OBOX_BVH)
				json["accelerator"] = "obox";
		}

		Json& materialsNode = json["materials"];
		for(auto& mat : m_materials)
		{
			Json& matNode = materialsNode[mat.second.getName()];
			matNode["type"] = mat.second.getType();
			for(auto& tex : mat.second.m_textureNames)
				matNode[tex.first] = tex.second;
			for(auto& val : mat.second.m_values)
			{
				auto& arrayNode = matNode[val.first];
				for(int i = 0; i < val.second.numComponents; ++i)
					arrayNode.push_back(val.second.values[i]);
			}
		}

		Json& lightsNode = json["lights"];
		for(auto& light : m_lights)
		{
			Json& lightNode = lightsNode[light->name];
			lightNode["type"] = Light::TypeToString(light->type);
			switch(light->type)
			{
			case Light::Type::POINT: {
				PointLight* l = dynamic_cast<PointLight*>(light.get());
				lightNode["position"] = {l->position.x, l->position.y, l->position.z};
				lightNode["intensity"] = {l->intensity.x, l->intensity.y, l->intensity.z};
			} break;
			case Light::Type::LAMBERT: {
				LambertLight* l = dynamic_cast<LambertLight*>(light.get());
				lightNode["position"] = {l->position.x, l->position.y, l->position.z};
				lightNode["intensity"] = {l->intensity.x, l->intensity.y, l->intensity.z};
				lightNode["normal"] = {l->normal.x, l->normal.y, l->normal.z};
			} break;
			case Light::Type::DIRECTIONAL: {
				DirectionalLight* l = dynamic_cast<DirectionalLight*>(light.get());
				lightNode["direction"] = {l->direction.x, l->direction.y, l->direction.z};
				lightNode["irradiance"] = {l->irradiance.x, l->irradiance.y, l->irradiance.z};
			} break;
			case Light::Type::SPOT: {
				SpotLight* l = dynamic_cast<SpotLight*>(light.get());
				lightNode["position"] = {l->position.x, l->position.y, l->position.z};
				lightNode["direction"] = {l->direction.x, l->direction.y, l->direction.z};
				lightNode["peakIntensity"] = {l->peakIntensity.x, l->peakIntensity.y, l->peakIntensity.z};
				lightNode["falloff"] = l->falloff;
				lightNode["halfAngle"] = l->halfAngle;
			} break;
			case Light::Type::SKY: {
				SkyLight* l = dynamic_cast<SkyLight*>(light.get());
				lightNode["sunDirection"] = {l->sunDirection.x, l->sunDirection.y, l->sunDirection.z};
				lightNode["turbidity"] = l->turbidity;
				lightNode["aerialPerspective"] = l->aerialPerspective;
			} break;
			case Light::Type::GONIOMETRIC: {
				GoniometricLight* l = dynamic_cast<GoniometricLight*>(light.get());
				lightNode["position"] = {l->position.x, l->position.y, l->position.z};
				lightNode["intensityScale"] = {l->intensityScale.x, l->intensityScale.y, l->intensityScale.z};
				lightNode["intensityMap"] = l->intensityMap;
			} break;
			case Light::Type::ENVIRONMENT: {
				EnvironmentLight* l = dynamic_cast<EnvironmentLight*>(light.get());
				lightNode["radianceMap"] = l->radianceMap;
			} break;
			}
			// Find all scenarios which reference this light
			auto& scenariosNode = lightNode["scenario"];
			for(auto & sc : m_scenarios)
				if(sc.hasLight(light))
					scenariosNode.push_back(sc.getName());
		}

		Json& camerasNode = json["cameras"];
		for(auto& cam : m_cameras)
		{
			Json& camNode = camerasNode[cam->name];
			camNode["type"] = Camera::TypeToString(cam->type);
			camNode["velocity"] = cam->velocity;
			switch(cam->type)
			{
			case Camera::Type::PERSPECTIVE: {
				PerspectiveCamera* c = dynamic_cast<PerspectiveCamera*>(cam.get());
				camNode["position"] = {c->position.x, c->position.y, c->position.z};
				camNode["lookAt"] = {c->lookAt.x, c->lookAt.y, c->lookAt.z};
				camNode["up"] = {c->up.x, c->up.y, c->up.z};
				camNode["fov"] = c->verticalFOV * 180.0f / 3.141592654f;
			} break;
			case Camera::Type::ORTHOGRAPHIC: {
				OrthographicCamera* c = dynamic_cast<OrthographicCamera*>(cam.get());
				camNode["position"] = {c->position.x, c->position.y, c->position.z};
				camNode["lookAt"] = {c->lookAt.x, c->lookAt.y, c->lookAt.z};
				camNode["up"] = {c->up.x, c->up.y, c->up.z};
				camNode["left"] = c->left;
				camNode["right"] = c->right;
				camNode["bottom"] = c->bottom;
				camNode["top"] = c->top;
				camNode["near"] = c->near;
				camNode["far"] = c->far;
			} break;
			case Camera::Type::FOCUS: {
				FocusCamera* c = dynamic_cast<FocusCamera*>(cam.get());
				camNode["position"] = {c->position.x, c->position.y, c->position.z};
				camNode["lookAt"] = {c->lookAt.x, c->lookAt.y, c->lookAt.z};
				camNode["up"] = {c->up.x, c->up.y, c->up.z};
				camNode["focalLength"] = c->focalLength;
				camNode["focusDistance"] = c->focusDistance;
				camNode["sensorSize"] = c->sensorSize;
				camNode["aperture"] = c->aperture;
			} break;
			}
			// Find all scenarios which reference this camera
			auto& scenariosNode = camNode["scenario"];
			for(auto & sc : m_scenarios)
				if(sc.getCamera() == cam)
					scenariosNode.push_back(sc.getName());
		}

		// pretty print with indent of 4 spaces
		envFile << std::setw(4) << json;
	}
}