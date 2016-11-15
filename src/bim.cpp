#include "bim.hpp"

namespace bim {

	BinaryModel::BinaryModel(Property::Val _properties) :
		m_numChunks(1),
		m_dimScale(1),
		m_chunkStates(1, ChunkState::LOADED),	// Chunk exists, but is empty (no mesh data)
		m_chunks(1),
		m_requestedProps(Property::Val(_properties | Property::POSITION | Property::TRIANGLE_IDX)),
		m_accelerator(Property::DONT_CARE),
		m_loadAll(false),
		m_numTrianglesPerLeaf(2)
	{
		m_chunks[0].m_properties = m_requestedProps;
		m_chunks[0].m_parent = this;
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

	int BinaryModel::findMaterial(const std::string& _name)
	{
		for(size_t i = 0; i < m_materialIndirection.size(); ++i)
		{
			if(m_materials[m_materialIndirection[i]].getName() == _name)
				return (int)i;
		}
		return -1;
	}

	void BinaryModel::split(const ei::IVec3& _numChunks)
	{
	}

	Scenario * BinaryModel::getScenario(uint _index)
	{
		if(m_scenarios.size() > _index)
			return &m_scenarios[_index];
		else return nullptr;
	}

	Scenario * BinaryModel::getScenario(const std::string & _name)
	{
		// Linear search for the scenario
		for(auto & e : m_scenarios)
			if(e.getName() == _name) return &e;
		return nullptr;
	}

	Scenario * BinaryModel::addScenario(const std::string & _name)
	{
#ifdef DEBUG
		if(getScenario(_name)) {
			std::cerr << "There is already a scenario with the same name!\n";
			return nullptr;
		}
#endif
		m_scenarios.push_back(Scenario(_name));
		return &m_scenarios.back();
	}

	std::shared_ptr<Light> BinaryModel::getLight(uint _index)
	{
		if(m_lights.size() > _index)
			return m_lights[_index];
		else return nullptr;
	}

	std::shared_ptr<Light> BinaryModel::getLight(const std::string & _name)
	{
		// Linear search for the light
		for(auto l : m_lights)
			if(l->name == _name) return l;
		return nullptr;
	}

	void BinaryModel::addLight(std::shared_ptr<Light> _light)
	{
#ifdef DEBUG
		if(getLight(_name)) {
			std::cerr << "There is already a light with the same name!\n";
			return nullptr;
		}
#endif
		m_lights.push_back(_light);
	}

} // namespace bim