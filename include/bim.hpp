#pragma once

#include "chunk.hpp"
#include "material.hpp"
#include <fstream>
#include <ei/3dtypes.hpp>

class Json;
struct JsonValue;

namespace bim {

	/// Main interface to load and access data in a .bim file.
	///
	/// A bim file contains a lot of arrays each representing a single property.
	///	A property can be vertex positions, normals, tangents, ... Not each file
	///	contains each property and the needed ones must be told on load (other are
	///	not loaded at all).
	class BinaryModel
	{
	public:
		BinaryModel(Property::Val _properties = Property::Val(Property::POSITION | Property::TRIANGLE_IDX));
		
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
		void storeChunk(const char* _bimFile, const ei::IVec3& _chunkPos);
		
		/// Split the scene into chunks. This operation cannot be undone.
		///	It is possible that triangles must be cut resulting in more geometry than
		///	before. Also, a split can never reduce the number of chunks in any
		///	direction.
		void split(const ei::IVec3& _numChunks);
		
		const ei::IVec3 getNumChunks() const { return m_numChunks; }
		Chunk* getChunk(const ei::IVec3& _chunkPos) { return &m_chunks[dot(_chunkPos, m_dimScale)]; }
		
		/// Check if a chunk is loaded and if not do it.
		void makeChunkResident(const ei::IVec3& _chunkPos);
		/// Schedule a load task if necessary.
		void makeChunkResidentAsync(const ei::IVec3& _chunkPos);
		bool isChunkResident(const ei::IVec3& _chunkPos) const;
		/// Mark a chunk as unused. It might get deleted if memory is required.
		void realeaseChunk(const ei::IVec3& _chunkPos);

		/// When editing the model bounding box is not always up to date. Make sure it is.
		void refreshBoundingBox();

		/// Get a material by its index (the same as used int TRIANGLE_MAT).
		/// The index is guaranteed to be non changing.
		Material& getMaterial(uint _index) { return m_materials[m_materialIndirection[_index]]; }
		const Material& getMaterial(uint _index) const { return m_materials[m_materialIndirection[_index]]; }
		void addMaterial(const Material& _material);
		uint getNumMaterials() const { return (uint)m_materials.size(); }
		/// Get the index of a named material. If the material is not found -1 is returned.
		int findMaterial(const std::string& _name);

		const ei::Box& getBoundingBox() const { return m_boundingBox; }
		/// Global parameter for the chunk->buildHierarchy().
		void setNumTrianglesPerLeaf(uint _numTrianglesPerLeaf) { m_numTrianglesPerLeaf = m_numTrianglesPerLeaf; }
		uint getNumTrianglesPerLeaf() const { return m_numTrianglesPerLeaf; }

		/// Returns the acceleration structure type as specified by the environment file.
		/// \return One of AABOX_BVH, OBOX_BVH or SPHERE_BVH.
		Property::Val getAccelerator() const { return m_accelerator; }
		/// Set one of AABOX_BVH, OBOX_BVH or SPHERE_BVH as the accelerator to be used.
		/// If the property does not exist this command will do nothing.
		void setAccelerator(Property::Val _accelerator) { if(m_chunks[0].m_properties & _accelerator) m_accelerator = _accelerator; }
	private:
		std::string loadEnv(const char* _envFile, bool _ignoreBinary);
		void loadMaterial(Json & json, const JsonValue & _matNode);

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
		std::vector<Material> m_materials;
		std::vector<uint> m_materialIndirection;
		Property::Val m_requestedProps;	///< All properties for which the getter should succeed.
		Property::Val m_optionalProperties;
		Property::Val m_accelerator;	///< Chosen kind of acceleration structure (specified by environment file)
		bool m_loadAll;					///< If a chunk is loaded, load all available data or only the required part
		ei::Box m_boundingBox;
		uint m_numTrianglesPerLeaf;		///< When the hierarchy is build the number of triangles per leaf node is set as parameter
	};

}

#include "exporter.hpp"