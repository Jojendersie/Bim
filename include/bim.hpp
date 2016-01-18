#pragma once

#include "chunk.hpp"
#include "material.hpp"
#include <fstream>
#include <ei/3dtypes.hpp>

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
		/// \param [in] _bimFile Name of the binary file containing the scene graph
		///		and mesh information.
		/// \param [in] _envFile A JSON file with material and lighting information.
		///	\param [in] _loadAll Also load properties which are not requested.
		/// \return true on success.
		bool load(const char* _bimFile, const char* _envFile, Property::Val _requiredProperties, bool _loadAll = false);
		
		/// Stores global information like material but without chunks.
		void store(const char* _bimFile, const char* _envFile);
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

		/// Helper structure to define inputs for appendVertex().
		/*struct PropDesc
		{
			BinaryModel::Property property;
			void* data;
		};
		/// Check if the given property array satisfies the needs of the model.
		bool validatePropertyDescriptors(PropDesc* _properties, int _num);*/
	private:
		bool loadEnv(const char* _envFile);

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
		Property::Val m_requestedProps;	///< All properties for which the getter should succeed.
		Property::Val m_loadedProps;	///< Available properties (a superset of m_requestedProps).
		ei::Box m_boundingBox;
	};

}

#include "exporter.hpp"