#include "bim.hpp"

namespace bim {

	BinaryModel::BinaryModel(Property::Val _properties) :
		m_numChunks(1),
		m_dimScale(1),
		m_chunkStates(1, ChunkState::LOADED),	// Chunk exists, but is empty (no mesh data)
		m_chunks(1),
		m_requestedProps(Property::Val(_properties | Property::POSITION)),
		m_loadedProps(Property::Val(_properties | Property::POSITION)),
		m_boundingBox()
	{
		m_chunks[0].m_properties = Property::Val(_properties | Property::POSITION);
	}

	/*bool BinaryModel::validatePropertyDescriptors(PropDesc* _properties, int _num)
	{
		Property provided = 0;
		for(int i = 0; i < _num; ++i)
			provided |= _properties[i].property;
		// Must contain all required
		if(provided & m_requestedProps != m_requestedProps)
			return false;
		// Should not contain more than loaded
		if(provided & m_loadedProps != provided)
			return false;
		return true;
	}*/

	void BinaryModel::split(const ei::IVec3& _numChunks)
	{
	}

} // namespace bim