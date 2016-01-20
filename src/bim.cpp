#include "bim.hpp"

namespace bim {

	BinaryModel::BinaryModel(Property::Val _properties) :
		m_numChunks(1),
		m_dimScale(1),
		m_chunkStates(1, ChunkState::LOADED),	// Chunk exists, but is empty (no mesh data)
		m_chunks(1),
		m_requestedProps(Property::Val(_properties | Property::POSITION | Property::TRIANGLE_IDX)),
		m_loadAll(false)
	{
		m_chunks[0].m_properties = m_requestedProps;
		m_boundingBox.min = ei::Vec3(1e10f);
		m_boundingBox.max = ei::Vec3(-1e10f);
	}

	void BinaryModel::refreshBoundingBox()
	{
		// Invariant: The bounding always represents all unloaded chunks.
		// If something changed it is only in the resident blocks.
		for(size_t i = 0; i < m_chunks.size(); ++i)
			if(m_chunkStates[i] == ChunkState::LOADED)
				m_boundingBox = ei::Box(m_boundingBox, m_chunks[i].m_boundingBox);
	}

	void BinaryModel::addMaterial(const Material& _material)
	{
		m_materialIndirection.push_back((uint32)m_materials.size());
		m_materials.push_back(_material);
	}

	void BinaryModel::split(const ei::IVec3& _numChunks)
	{
	}

} // namespace bim