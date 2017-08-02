#pragma once

#include "chunk.hpp"
#include "material.hpp"
#include "scenario.hpp"
#include "camera.hpp"
#include "../deps/json/json_fwd.hpp"
#include <fstream>
#include <ei/3dtypes.hpp>

namespace bim {

	using Json = nlohmann::basic_json<std::map, std::vector, std::string, bool, std::int64_t,
		std::uint64_t, float, std::allocator, nlohmann::adl_serializer>;

	/// Main interface to load and access data in a .bim file.
	///
	/// A bim file contains a lot of arrays each representing a single property.
	///	A property can be vertex positions, normals, tangents, ... Not each file
	///	contains each property and the needed ones must be told on load (other are
	///	not loaded at all).
	class BinaryModel
	{
	public:
		/// Create an empty model.
		/// All chunks count as empty at the beginning. They must be made resident
		/// before any use (write or load).
		/// \param [in] _properties A number of attributes which should be defined for this
		///		model. Some attributes like a tangent space can be added later.
		///		The default (and required attributes) are POSITION and TRIANGLE_IDX.
		/// \param [in] _numChunks Build a splitted scene for out of core purposes.
		///		Each chunk is an independent full renderable scene with BVH, ....
		///		The subdivision into chunks cannot be changed.
		explicit BinaryModel(Property::Val _properties = Property::Val(Property::POSITION | Property::TRIANGLE_IDX), const ei::IVec3& _numChunks = ei::IVec3(1));
		
		/// Preload the model meta informations.
		/// To truly load the data call makeChunkResident() for the portions you need.
		/// \param [in] _envFile A JSON file with material and lighting information.
		/// \param [in] _optionalProperties If existent load these properties, but do not create empty defaults.
		///	\param [in] _loadAll Also load properties which are not requested.
		/// \return true on success. Fails, if the _envFile does not reference a valid binary file.
		bool load(const char* _envFile, Property::Val _requiredProperties, Property::Val _optionalProperties = Property::DONT_CARE, bool _loadAll = false);
		/// Load the json file with material, lighting,... informations.
		/// Referenced binary data will be ignored.
		/// \param [in] _envFile A JSON file.
		void loadEnvironmentFile(const char* _envFile);
		
		/// Stores global information like material and lights.
		/// \param [in] _bimFile Name of the binary file which should be referenced by
		///		the environment (scene) file.
		void storeEnvironmentFile(const char* _envFile, const char* _bimFile);
		/// Store meta file chunks (e.g. number of chunks) into the binary file.
		/// This must be called before any chunk is stored.
		void storeBinaryHeader(const char* _bimFile);
		/// Appends a chunk to the file (expecting the other information already exist).
		/// Chunks must be written in grid order (x-fastest/thickly packed, then y, then z).
		void storeChunk(const char* _bimFile, const ei::IVec3& _chunkPos);

		const ei::IVec3& getNumChunks() const { return m_numChunks; }
		Chunk* getChunk(const ei::IVec3& _chunkPos);
		
		/// Check if a chunk is loaded and if not do it.
		void makeChunkResident(const ei::IVec3& _chunkPos);
		/// Schedule a load task if necessary.
		void makeChunkResidentAsync(const ei::IVec3& _chunkPos);
		bool isChunkResident(const ei::IVec3& _chunkPos) const;
		/// Mark a chunk as unused. It might get deleted if memory is required.
		void realeaseChunk(const ei::IVec3& _chunkPos);
		/// Definitely remove the chunk from memory.
		void deleteChunk(const ei::IVec3& _chunkPos);

		/// When editing the model bounding box is not always up to date. Make sure it is.
		void refreshBoundingBox();

		/// Get a material by its index (the same as used int TRIANGLE_MAT).
		/// The index is guaranteed to be non changing.
		Material* getMaterial(uint _index) { auto it = m_materials.find(m_materialIndirection[_index]); if(it != m_materials.end()) return &it->second; else return nullptr; }
		const Material* getMaterial(uint _index) const { auto it = m_materials.find(m_materialIndirection[_index]); if(it != m_materials.end()) return &it->second; else return nullptr; }
		Material* addMaterial(const Material& _material);
		uint getNumUsedMaterials() const { return static_cast<uint>(m_materialIndirection.size()); }
		/// Get the index of a named material. If the material is not found -1 is returned.
		int getUniqueMaterialIndex(const std::string& _name);
		Material* getMaterial(const std::string& _name) { auto it = m_materials.find(_name); if(it != m_materials.end()) return &it->second; else return nullptr; }
		const Material* getMaterial(const std::string& _name) const { auto it = m_materials.find(_name); if(it != m_materials.end()) return &it->second; else return nullptr; }

		const ei::Box& getBoundingBox() const { return m_boundingBox; }
		/// Allows an external update of the bounding box for out of core building purposes.
		/// (Bounding box must be known in advance).
		void setBoundingBox(const ei::Box& _box) { m_boundingBox = _box; }

		/// Returns the acceleration structure type as specified by the environment file.
		/// \return One of AABOX_BVH, OBOX_BVH or SPHERE_BVH.
		Property::Val getAccelerator() const { return m_accelerator; }
		/// Set one of AABOX_BVH, OBOX_BVH or SPHERE_BVH as the accelerator to be used.
		/// If the property does not exist this command will do nothing.
		void setAccelerator(Property::Val _accelerator) { if(m_chunks[0].m_properties & _accelerator) m_accelerator = _accelerator; }

		uint getNumScenarios() const { return static_cast<uint>(m_scenarios.size()); }
		/// Scenarios can be accessed by index or by name (by index is faster)
		Scenario* getScenario(uint _index);
		Scenario* getScenario(const std::string& _name);
		/// Create a new scenario and obtain its reference
		Scenario* addScenario(const std::string& _name);

		uint getNumLights() const { return static_cast<uint>(m_lights.size()); }
		/// Lights can be accessed by index or by name (by index is faster)
		std::shared_ptr<Light> getLight(uint _index);
		std::shared_ptr<const Light> getLight(uint _index) const { return const_cast<BinaryModel*>(this)->getLight(_index); }
		std::shared_ptr<Light> getLight(const std::string& _name);
		std::shared_ptr<const Light> getLight(const std::string& _name) const { return const_cast<BinaryModel*>(this)->getLight(_name); }
		void addLight(std::shared_ptr<Light> _light);
	private:
		std::string loadEnv(const char* _envFile, bool _ignoreBinary);
		void loadMaterial(const Json& _node, const std::string& _name);
		void loadLight(const Json& _node, const std::string& _name);
		void loadCamera(const Json& _node, const std::string& _name);

		enum class ChunkState {
			LOADED,
			EMPTY,
			LOAD_REQUEST,
			RELEASE_REQUEST,	///< Counts as empty for isChunkResident()
		};
		
		std::ifstream m_file;			///< Permanent access to the file
		ei::IVec3 m_numChunks;
		ei::IVec3 m_dimScale;			///< Vector to transform 3D index into 1D (1, m_numChunks.x, m_numChunks.x*m_numChunks.y)
		std::vector<ChunkState> m_chunkStates; // TODO: make atomic
		std::vector<Chunk> m_chunks;
		std::unordered_map<std::string, Material> m_materials;
		std::vector<std::string> m_materialIndirection;
		std::vector<std::shared_ptr<Light>> m_lights;
		std::vector<std::shared_ptr<Camera>> m_cameras;
		std::vector<Scenario> m_scenarios;
		Property::Val m_requestedProps;	///< All properties for which the getter should succeed.
		Property::Val m_optionalProperties;
		Property::Val m_accelerator;	///< Chosen kind of acceleration structure (specified by environment file)
		bool m_loadAll;					///< If a chunk is loaded, load all available data or only the required part
		ei::Box m_boundingBox;
	};

}

#include "exporter.hpp"