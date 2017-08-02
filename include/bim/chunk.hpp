#pragma once

#include <ei/3dtypes.hpp>
#include <ei/stdextensions.hpp>
#include <vector>

namespace bim {
	
	struct Property
	{
		enum Val {
			DONT_CARE		= 0,
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
			TRIANGLE_IDX	= 0x00010000,	///< The three indices of vertices
			TRIANGLE_MAT	= 0x00020000,	///< One index for the material
			
			// Hierarchy Properties:
			AABOX_BVH		= 0x01000000,	///< Axis aligned bounding boxes for the hierarchy
			OBOX_BVH		= 0x02000000,
			SPHERE_BVH		= 0x04000000,
			HIERARCHY		= 0x08000000,	///< Node and Leaves array for the hierarchy
			NDF_SGGX		= 0x10000000,	///< Normal distribution functions for the hierarchy in SGGX basis
		};
	};
	
	struct Node
	{
//		uint32 parent;		///< Index of the parent node
		uint32 firstChild;	///< Index of the first child node
		uint32 escape;		///< Index of the next element in a preorder traversal if children are skipped. This can be a sibling or some node on a higher level.
	};

	/// A simplification of a node by SGGX base function.
	/// \details This stores the encoded entries of a symmetric matrix S:
	///		σ = (sqrt(S_xx), sqrt(S_yy), sqrt(S_zz))
	///		r = (S_xy/sqrt(S_xx S_yy), S_xz/sqrt(S_xx S_zz), S_yz/sqrt(S_yy S_zz))
	///
	///		To reconstruct the matrix do the following:
	///		S_xx = σ.x^2    S_yy = σ.y^2    S_zz = σ.z^2
	///		S_xy = r.x * σ.x * σ.y
	///		S_xz = r.y * σ.x * σ.z
	///		S_yz = r.z * σ.y * σ.z
	struct SGGX
	{
		ei::Vec<uint16, 3> σ;		///< Values in [0,1] discretized to 16 bit
		ei::Vec<uint16, 3> r;		///< Values in [-1,1] discretized to 16 bit (the interval is shifted by *0.5-0.5 to fit the same format like σ)
	};


	class Chunk
	{
	public:
		Chunk(class BinaryModel* _parent = nullptr);

		uint getNumVertices() const					{ return (uint)m_positions.size(); }
		ei::Vec3* getPositions()					{ return m_positions.empty() ? nullptr : m_positions.data(); }
		const ei::Vec3* getPositions() const		{ return m_positions.empty() ? nullptr : m_positions.data(); }
		ei::Vec3* getNormals()						{ return m_normals.empty() ? nullptr : m_normals.data(); }
		const ei::Vec3* getNormals() const			{ return m_normals.empty() ? nullptr : m_normals.data(); }
		ei::Vec3* getTangents()						{ return m_tangents.empty() ? nullptr : m_tangents.data(); }
		const ei::Vec3* getTangents() const			{ return m_tangents.empty() ? nullptr : m_tangents.data(); }
		ei::Vec3* getBitangents()					{ return m_bitangents.empty() ? nullptr : m_bitangents.data(); }
		const ei::Vec3* getBitangents() const		{ return m_bitangents.empty() ? nullptr : m_bitangents.data(); }
		ei::Quaternion* getQormals()				{ return m_qormals.empty() ? nullptr : m_qormals.data(); }
		const ei::Quaternion* getQormals() const	{ return m_qormals.empty() ? nullptr : m_qormals.data(); }
		ei::Vec2* getTexCoords0()					{ return m_texCoords0.empty() ? nullptr : m_texCoords0.data(); }
		const ei::Vec2* getTexCoords0() const		{ return m_texCoords0.empty() ? nullptr : m_texCoords0.data(); }
		ei::Vec2* getTexCoords1()					{ return m_texCoords1.empty() ? nullptr : m_texCoords1.data(); }
		const ei::Vec2* getTexCoords1() const		{ return m_texCoords1.empty() ? nullptr : m_texCoords1.data(); }
		ei::Vec2* getTexCoords2()					{ return m_texCoords2.empty() ? nullptr : m_texCoords2.data(); }
		const ei::Vec2* getTexCoords2() const		{ return m_texCoords2.empty() ? nullptr : m_texCoords2.data(); }
		ei::Vec2* getTexCoords3()					{ return m_texCoords3.empty() ? nullptr : m_texCoords3.data(); }
		const ei::Vec2* getTexCoords3() const		{ return m_texCoords3.empty() ? nullptr : m_texCoords3.data(); }
		uint32* getColors()							{ return m_colors.empty() ? nullptr : m_colors.data(); }
		const uint32* getColors() const				{ return m_colors.empty() ? nullptr : m_colors.data(); }

		uint getNumTriangles() const				{ return (uint)m_triangles.size(); }
		ei::UVec3* getTriangles()					{ return m_triangles.empty() ? nullptr : m_triangles.data(); }
		const ei::UVec3* getTriangles() const		{ return m_triangles.empty() ? nullptr : m_triangles.data(); }
		uint32* getTriangleMaterials()				{ return m_triangleMaterials.empty() ? nullptr : m_triangleMaterials.data(); }
		const uint32* getTriangleMaterials() const	{ return m_triangleMaterials.empty() ? nullptr : m_triangleMaterials.data(); }

		uint getNumNodes() const					{ return (uint)m_hierarchy.size(); }
		uint getNumTreeLevels() const				{ return m_numTreeLevels; }
		uint getNumLeaves() const					{ return (uint)(m_hierarchyLeaves.size()); }
		Node* getHierarchy()						{ return m_hierarchy.empty() ? nullptr : m_hierarchy.data(); }
		const Node* getHierarchy() const			{ return m_hierarchy.empty() ? nullptr : m_hierarchy.data(); }
		uint32* getHierarchyParents()				{ return m_hierarchyParents.empty() ? nullptr : m_hierarchyParents.data(); }
		const uint32* getHierarchyParents() const	{ return m_hierarchyParents.empty() ? nullptr : m_hierarchyParents.data(); }
		const ei::Box* getHierarchyAABoxes() const	{ return m_aaBoxes.data(); }
		const ei::OBox* getHierarchyOBoxes() const	{ return m_oBoxes.data(); }
		const ei::UVec4* getLeafNodes() const		{ return m_hierarchyLeaves.data(); }
		const SGGX* getNodeNDFs() const				{ return m_nodeNDFs.empty() ? nullptr : m_nodeNDFs.data();}

		struct FullVertex
		{
			ei::Vec3 position;
			ei::Vec3 normal;
			ei::Vec3 tangent;
			ei::Vec3 bitangent;
			ei::Quaternion qormal;
			ei::Vec2 texCoord0;
			ei::Vec2 texCoord1;
			ei::Vec2 texCoord2;
			ei::Vec2 texCoord3;
			ei::uint32 color;
			
			FullVertex();
			bool operator == (const FullVertex& _rhs) const;
		};

		/// Add the data for an entire vertex. A vertex should contain the same set
		///	of properties as given on load/construction. All other properties are
		/// filled with defaults.
		///
		/// Example:
		/// bim::Chunk::FullVertexPropertyMap descriptor;
		/// for(...) {
		///		descriptor.position = ...
		///		descriptor.normal = ...
		///		chunk.addVertex(descriptor);
		///		...
		///		chunk.addTriangle(UVec3(...), matID);
		///	}
		void addVertex(const FullVertex& _properties);

		/// Overwrite a specific vertex.
		/// If there are less vertices than _index the internal memory is resized.
		void setVertex(uint32 _index, const FullVertex& _properties);
		
		void addTriangle(const ei::UVec3& _indices, uint32 _material);
		
		/// Tries to match vertices with a hash map and rebuilds the index buffer.
		void removeRedundantVertices();

		/// Recomputes normals, ... dependent on which of the properties are used
		///	in the current model.
		/// \param [in] _components Flags for the tangent space representations
		///		which should be computed. NORMAL, TANGENT, BITANGENT and QORMAL are
		///		valid.
		/// \param [in] _preserveOriginals Preserves vectors which have a length of
		///		one. These are assumed to be loaded from the file. Non existing or
		///		invalid vectors are recomputed.
		void computeTangentSpace(Property::Val _components, bool _preserveOriginals);
		/// Change the sign of the normal if winding order is different than expected.
		/// This does not change the winding order itself. NORMAL and QORMAL are modified.
		void flipNormals();

		enum class BuildMethod
		{
			KD_TREE,	///< Sort once in all directions, then recursively split at median
			SAH,		///< Use surface area heuristic in the 'largest' dimension.
			SBVH,		///< "Spatial Splits in Bounding Volume Hierarchies". Results in more nodes with less overlap by partial reference duplication. Other than that it uses SAH too.
		};
		/// Build a hierarchy on top of all triangles
		void buildHierarchy(BuildMethod _method, uint _maxNumTrianglesPerLeaf);
		/// Compute bounding volumes for all nodes in the hierarchy.
		void computeBVHAABoxes();
		void computeBVHOBoxes();
		void computeBVHSpheres();

		void computeBVHSGGXApproximations();

	private: friend class BinaryModel;
		class BinaryModel* m_parent;
		uint64 m_address;
		Property::Val m_properties;
		ei::Box m_boundingBox;
		std::vector<ei::Vec3> m_positions;
		std::vector<ei::Vec3> m_normals;
		std::vector<ei::Vec3> m_tangents;
		std::vector<ei::Vec3> m_bitangents;
		std::vector<ei::Quaternion> m_qormals;
		std::vector<ei::Vec2> m_texCoords0;
		std::vector<ei::Vec2> m_texCoords1;
		std::vector<ei::Vec2> m_texCoords2;
		std::vector<ei::Vec2> m_texCoords3;
		std::vector<ei::uint32> m_colors;
		std::vector<ei::UVec3> m_triangles;
		std::vector<uint32> m_triangleMaterials;
		std::vector<Node> m_hierarchy;				///< Child and escape pointers. Defined if Property::HIERARCHY is available.
		std::vector<uint32> m_hierarchyParents;		///< Indices of the parent nodes. Defined if Property::HIERARCHY is available.
		std::vector<ei::UVec4> m_hierarchyLeaves;
		std::vector<ei::Box> m_aaBoxes;
		std::vector<ei::OBox> m_oBoxes;
		std::vector<SGGX> m_nodeNDFs;
		uint m_numTreeLevels;

		// Allocate space for a certain property and initialize to defaults.
		// If the property already exists nothing is done.
		void addProperty(Property::Val _property);

		// Delete all hierarchy information, because it is outdated.
		void invalidateHierarchy();
		
		// Flip qormals to align them within each triangle.
		void unifyQormals();

		void buildBVH_kdtree(uint _maxNumTrianglesPerLeaf);
		void buildBVH_SAHsplit(uint _maxNumTrianglesPerLeaf);
		void buildBVH_SBVH(uint _maxNumTrianglesPerLeaf);
		// All build methods must write left->firstChild and right->escape. After
		// the primary build the remap iterates the tree once and replaces all pointers
		// by the correct ones.
		// Returns the maximum tree depth.
		uint remapNodePointers(uint32 _this, uint32 _parent, uint32 _escape);
	};
	inline const ei::Vec3& positionOf(const Chunk::FullVertex& _vertex) { return _vertex.position; }

} // namespace bim

namespace std {
	template<> struct hash<bim::Chunk::FullVertex>
	{
		size_t operator()(const bim::Chunk::FullVertex& x) const
		{
			size_t h = hash<ei::Vec3>()(x.position);
			h ^= hash<ei::Vec3>()(x.normal);
			h ^= hash<ei::Vec3>()(x.tangent);
			h ^= hash<ei::Vec3>()(x.bitangent);
			h ^= hash<ei::Vec4>()(*reinterpret_cast<const ei::Vec4*>(&x.qormal));
			h ^= hash<ei::Vec2>()(x.texCoord0);
			h ^= hash<ei::Vec2>()(x.texCoord1);
			h ^= hash<ei::Vec2>()(x.texCoord2);
			h ^= hash<ei::Vec2>()(x.texCoord3);
			h ^= x.color;
			return h;
		}
	};
}