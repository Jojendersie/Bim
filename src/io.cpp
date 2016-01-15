#include "bim.hpp"
#include <fstream>
#include <iostream>

namespace bim {

	struct SectionHeader
	{
		uint32 type;	// BinaryModel::Property::Val + extra values
		uint64 size;	// Block size of following data
	};

	const int META_SECTION = 0x0;
	const int CHUNK_SECTION = 0x3;

	struct MetaSection
	{
		uint32 propertyMask;	// Flags for all available properties
		ei::IVec3 numChunks;	// Number of stored chunks
		ei::Box boundingBox;	// Entire scene bounding box
	};

	bool BinaryModel::load(const char* _bimFile, const char* _envFile, Property::Val _requiredProperties, bool _loadAll)
	{
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
		if((meta.propertyMask & _requiredProperties) != _requiredProperties)
		{
			std::cerr << "File does not contain the requested properties!\n";
			return false;
		}
		m_numChunks = meta.numChunks;
		m_dimScale = ei::IVec3(1, m_numChunks.x, m_numChunks.x * m_numChunks.y);
		m_requestedProps = _requiredProperties;
		m_loadedProps = Property::Val(_loadAll ? meta.propertyMask : _requiredProperties);
		m_boundingBox = meta.boundingBox;
	
		m_chunks.clear();
		Chunk emptyChunk;
		emptyChunk.m_properties = m_loadedProps;
		while(!m_file.eof())
		{
			m_file.read(reinterpret_cast<char*>(&header), sizeof(SectionHeader));
			if(header.type == CHUNK_SECTION)
			{
				int x = sizeof(m_file.tellg());
				emptyChunk.m_address = m_file.tellg();
				m_chunks.push_back(emptyChunk);
			}
		}
	
		if(m_chunks.size() != prod(m_numChunks))
		{
			std::cerr << "Missing chunks!\n";
			return false;
		}
	
		return true;
	}

	void BinaryModel::store(const char* _bimFile, const char* _envFile)
	{
		std::ofstream file(_bimFile, std::ios_base::binary);
		if(m_file.fail()) {std::cerr << "Cannot open file for writing!\n"; return;}
		SectionHeader header;
	
		header.type = META_SECTION;
		header.size = sizeof(MetaSection);
		file.write(reinterpret_cast<char*>(&header), sizeof(SectionHeader));
	
		MetaSection meta;
		meta.propertyMask = m_loadedProps;
		meta.numChunks = m_numChunks;
		meta.boundingBox = m_boundingBox;
		file.write(reinterpret_cast<char*>(&meta), sizeof(MetaSection));
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
			m_file.seekg(m_chunks[idx].m_address);
			// Now read the real data
			SectionHeader header;
			m_file.read(reinterpret_cast<char*>(&header), sizeof(SectionHeader));
			while(header.type != CHUNK_SECTION && !m_file.eof())
			{
				// Should this property be loaded?
				if((m_loadedProps & header.type) != 0)
				{
					switch(header.type)
					{
						case Property::POSITION:
							// ASSERT: _size % sizeof(ei::Vec3) == 0
							// Reserve the exact amount of memory
							swap(m_chunks[idx].m_positions, std::vector<ei::Vec3>(header.size / sizeof(ei::Vec3)));
							m_file.read(reinterpret_cast<char*>(m_chunks[idx].m_positions.data()), header.size);
							break;
						case Property::NORMAL:
							swap(m_chunks[idx].m_normals, std::vector<ei::Vec3>(header.size / sizeof(ei::Vec3)));
							m_file.read(reinterpret_cast<char*>(m_chunks[idx].m_normals.data()), header.size);
							break;
						default: m_file.seekg(header.size, std::ios_base::cur);
					}
				} else m_file.seekg(header.size, std::ios_base::cur);
				m_file.read(reinterpret_cast<char*>(&header), sizeof(SectionHeader));
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
		m_chunkStates[dot(m_dimScale, _chunk)] = ChunkState::RELEASE_REQUEST;
	}


	void BinaryModel::storeChunk(const char* _bimFile, const ei::IVec3& _chunkPos)
	{
		if(!isChunkResident(_chunkPos)) {std::cerr << "Chunk is not resident and cannot be stored!\n"; return;}
		int idx = dot(m_dimScale, _chunkPos);
		std::ofstream file(_bimFile, std::ios_base::binary | std::ios_base::app);
		if(m_file.fail()) {std::cerr << "Cannot open file for writing a chunk!\n"; return;}
	
		SectionHeader header;
		header.type = CHUNK_SECTION;
		header.size = m_chunks[idx].m_positions.size() * sizeof(ei::Vec3) +
					  m_chunks[idx].m_normals.size() * sizeof(ei::Vec3);
		file.write(reinterpret_cast<char*>(&header), sizeof(SectionHeader));
	
		header.type = Property::POSITION;
		header.size = m_chunks[idx].m_positions.size() * sizeof(ei::Vec3);
		file.write(reinterpret_cast<char*>(&header), sizeof(SectionHeader));
		file.write(reinterpret_cast<char*>(m_chunks[idx].m_positions.data()), header.size);

		if(m_loadedProps & Property::NORMAL)
		{
			header.type = Property::NORMAL;
			header.size = m_chunks[idx].m_normals.size() * sizeof(ei::Vec3);
			file.write(reinterpret_cast<char*>(&header), sizeof(SectionHeader));
			file.write(reinterpret_cast<char*>(m_chunks[idx].m_normals.data()), header.size);
		}
	}

}