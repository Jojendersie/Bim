#pragma once

/// Main interface to load and access data in a .bim file.
///
/// A bim file contains a lot of arrays each representing a single property.
///	A property can be vertex positions, normals, tangents, ... Not each file
///	contains each property and the needed ones must be told on load (other are
///	not loaded at all).
class BinaryModel
{
public:
	BinaryModel();
	
	struct Property
	{
		enum Val {
			// Vertex Properties:
			POSITION		= 0x00000001,
			NORMAL			= 0x00000002,
			TANGENT			= 0x00000004,
			BITANGENT		= 0x00000008,
			QORMAL			= 0x00000010,	///< Compressed tangent space in Quaternion form.
			//COMPRESSED_TS	= 0x00000400,	///< Compressed tangent space. This stores (phi, cos(theta)) for normal and tangent vector. The sign of the bitangent (cross(normal, tangent)) is encoded in phi_t [-2pi,2pi]. The reconstruction is made by n=(sin(theta_n)*sin(phi_n), sin(theta_n)*cos(phi_n), cos(theta_n)), t=(sin(theta_t)*sin(phi_t), sin(theta_t)*cos(phi_t), cos(theta_t)) and b = cross(n,t) * sign(phi_t).
			TEXCOORD0		= 0x00000020,
			TEXCOORD1		= 0x00000040,
			TEXCOORD2		= 0x00000080,
			TEXCOORD3		= 0x00000100,
			COLOR			= 0x00000200,
			
			// Triangle Properties:
			TRIANGLE_MAT	= 0x00010000,	///< Four indices (3 to vertices, one to the material)
			
			// Hierarchy Properties:
			AABOX_BVH		= 0x10000000,
			OBOX_BVH		= 0x20000000,
			SPHERE_BVH		= 0x40000000,
			ANY_BVH			= 0x80000000
		};
	};
	
	struct Node
	{
		uint32 parent;		///< Index of the parent node
		uint32 firstChild;	///< Index of the first child node
		uint32 escape;		///< Index of the next element in a preorder traversal if children are skipped. This can be a sibling or some node on a higher level.
	};
	
	/// \param [in] _bimFile Name of the binary file containing the scene graph
	///		and mesh information.
	/// \param [in] _matFile A JSON file with material information.
	///	\param [in] _loadAll Also load properties which are not requested.
	/// \return true on success.
	bool load(const char* _bimFile, const char* _matFile, Property::Val _requiredProperties, bool _loadAll = false);
	
	void store(const char* _bimFile, const char* _matFile);
	
	/// Split the scene into chunks. This operation cannot be undone.
	///	It is possible that triangles must be cut resulting in more geometry than
	///	before. Also, a split can never reduce the number of chunks in any
	///	direction.
	void split(const ei::IVec3& _numChunks);
	
	const ei::IVec3 getNumChunks() const { return m_numChunks; }
	/// Check if a chunk is loaded and if not do it.
	void makeChunkResident(const ei::IVec3& _chunk);
	/// Schedule a load task if necessary.
	void makeChunkResidentAsync(const ei::IVec3& _chunk);
	bool isChunkResident() const;
	/// Mark a chunk as unused. It might get deleted if memory is required.
	void realeaseChunk(const ei::IVec3& _chunk);
	
	ei::Vec3* getPositions(const ei::IVec3& _chunk);
	const ei::Vec3* getPositions(const ei::IVec3& _chunk) const;
	ei::Vec3* getNormals(const ei::IVec3& _chunk);
	const ei::Vec3* getNormals(const ei::IVec3& _chunk) const;
	ei::Quaternion* getQormals(const ei::IVec3& _chunk);
	const ei::Quaternion* getQormals(const ei::IVec3& _chunk) const;

	ei::UVec4* getTriangles(const ei::IVec3& _chunk);
	const ei::UVec4* getTriangles(const ei::IVec3& _chunk) const;

	Node* getHierarchy(const ei::IVec3& _chunk);
	const Node* getHierarchy(const ei::IVec3& _chunk) const;
	
	/// Helper structure to define inputs for appendVertex().
	struct PropDesc
	{
		BinaryModel::Property property;
		void* data;
	};
	/// Check if the given property array satisfies the needs of the model.
	bool validatePropertyDescriptors(PropDesc* _properties, int _num);
	
	/// Add the data for an entire vertex. A vertex must contain the same set
	///	of properties as given on load/construction.
	///
	/// Example:
	///	Vertex v; // buffer for own structured data for easier use
	/// BinaryModel::PropDesc descriptors[] = {
	/// 	{BinaryModel::Property::POSITION, &v.position},
	///		{BinaryModel::Property::NORMAL, &v.normal}};
	///	if(model.validatePropertyDescriptors(descriptors, 2)) {
	/// 	for(...) {
	///			v.position = ...
	///			v.normal = ...
	///			model.appendVertex(descriptors, 2);
	///			model.addTriangle(...);
	///		}
	///	}
	///
	/// // Refresh the split (new data is not sorted into chunks
	///	model.split(model.getNumChunks());
	void appendVertex(PropDesc* _properties, int _num);
	
	void addTriangle(const ei::UVec3& _indices, uint32 _material);
	
	/// Recomputes normals, ... dependent on which of the properties are used
	///	in the current model.
	void computeTangentSpace();
	
	void rebuildHierarchy(/*Build options*/);
private:
	enum class ChunkState{
		LOADED,
		EMPTY,
		LOAD_REQUEST,
		RELEASE_REQUEST,	///< Counts as empty for isChunkResident()
	};

	ei::IVec3 m_numChunks;
	std::vector<uint64> m_chunkAddresses;
	Property::Val m_requestedProps;	///< All properties for which the getter should succeed.
	Property::Val m_loadedProps;	///< Available properties (a superset of m_requestedProps).
	ei::Box m_boundingBox;
	bool m_hasValidHierarchy;
	
	/// Flip qormals to align them within each triangle.
	void unifyQormals();
};