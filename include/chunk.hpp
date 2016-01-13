#pragma once

#include <vector>

namespace bim {
	
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

	class Chunk
	{
	public:
		ei::Vec3* getPositions();
		const ei::Vec3* getPositions() const;
		ei::Vec3* getNormals();
		const ei::Vec3* getNormals() const;
		ei::Quaternion* getQormals();
		const ei::Quaternion* getQormals() const;

		ei::UVec4* getTriangles();
		const ei::UVec4* getTriangles() const;

		Node* getHierarchy();
		const Node* getHierarchy() const;
		
		struct VertexPropertyMap
		{
			const ei::Vec3* position;
			const ei::Vec3* normal;
			const ei::Vec3* tangent;
			const ei::Vec3* bitangent;
			const ei::Quaternion* qormal;
			const ei::Vec2* texCoord0;
			const ei::Vec2* texCoord1;
			const ei::Vec2* texCoord2;
			const ei::Vec2* texCoord3;
			const ei::uint32* color;
			
			VertexPropertyMap() { memset(this, 0, sizeof(VertexPropertyMap)); }
		};
		
		/// Add the data for an entire vertex. A vertex should contain the same set
		///	of properties as given on load/construction. All other properties are
		/// filled with defaults.
		///
		/// Example:
		///	Vertex v; // buffer for own structured data for easier use
		/// bim::Chunk::VertexPropertyMap descriptor;
		/// descriptor.position = &v.position;
		///	descriptor.normal = &v.normal;
		/// for(...) {
		///		v.position = ...
		///		v.normal = ...
		///		chunk.addVertex(descriptor);
		///		...
		///		chunk.addTriangle(UVec3(...), matID);
		///	}
		void addVertex(VertexPropertyMap* _properties);
		
		void addTriangle(const ei::UVec3& _indices, uint32 _material);
		
		/// Tries to match vertices with a hash map and rebuilds the index buffer.
		void removeRedundantVertices();

		/// Recomputes normals, ... dependent on which of the properties are used
		///	in the current model.
		void computeTangentSpace();
		
		void rebuildHierarchy(/*Build options*/);
	private: friend class BinaryModel;
		uint64 m_address;
		Property::Val m_properties;
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
		std::vector<ei::UVec4> m_triangles;
		bool m_hasValidHierarchy;
		
		/// Flip qormals to align them within each triangle.
		void unifyQormals();
	};

}