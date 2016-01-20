#include "chunk.hpp"

using namespace ei;

namespace bim {
	
	void Chunk::remapNodePointers(uint32 _this, uint32 _parent, uint32 _escape)
	{
		// Keep firstChild, because firstChild == left in any case.
		uint32 left = m_hierarchy[_this].firstChild;
		uint32 right = m_hierarchy[_this].escape;
		m_hierarchy[_this].parent = _parent;
		m_hierarchy[_this].escape = _escape;
		if(!(left & 0x80000000)) // If not a leaf-child
		{
			remapNodePointers(left, _this, right);
			remapNodePointers(right, _this, _escape);
		}
	}

	static void recomputeBVHAABoxesRec(const Vec3* _positions, const UVec4* _leaves, const Node* _hierarchy, Box* _aaBoxes, uint32 _node, uint _numTrianglesPerLeaf)
	{
		uint32 child = _hierarchy[_node].firstChild;
		if(child & 0x80000000)
		{
			// Build a box for all triangles in the leaf
			size_t leafIdx = child & 0x7fffffff;
			leafIdx *= _numTrianglesPerLeaf;
			Triangle tri;
			tri.v0 = _positions[_leaves[leafIdx].x];
			tri.v1 = _positions[_leaves[leafIdx].y];
			tri.v2 = _positions[_leaves[leafIdx].z];
			_aaBoxes[_node] = Box(tri);
			for(uint i = 1; i < _numTrianglesPerLeaf; ++i)
			{
				tri.v0 = _positions[_leaves[leafIdx+i].x];
				tri.v1 = _positions[_leaves[leafIdx+i].y];
				tri.v2 = _positions[_leaves[leafIdx+i].z];
				_aaBoxes[_node] = Box(Box(tri), _aaBoxes[_node]);
			}
		} else {
			// Iterate through all siblings
			recomputeBVHAABoxesRec(_positions, _leaves, _hierarchy, _aaBoxes, child, _numTrianglesPerLeaf);
			_aaBoxes[_node] = _aaBoxes[child];
			child = _hierarchy[child].escape;
			while(_hierarchy[child].parent == _node && child != 0)
			{
				recomputeBVHAABoxesRec(_positions, _leaves, _hierarchy, _aaBoxes, child, _numTrianglesPerLeaf);
				_aaBoxes[_node] = Box(_aaBoxes[child], _aaBoxes[_node]);
				child = _hierarchy[child].escape;
			}
		}
	}

	void Chunk::recomputeBVHAABoxes()
	{
		m_aaBoxes.resize(m_hierarchy.size());
		recomputeBVHAABoxesRec(m_positions.data(), m_hierarchyLeaves.data(), m_hierarchy.data(), m_aaBoxes.data(), 0, m_numTrianglesPerLeaf);
	}

} // namespace bim