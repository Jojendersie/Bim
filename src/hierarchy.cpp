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

	static void recomputeBVHAABoxesRec(const Node* _hierarchy, Box* _aaBoxes, uint32 _node)
	{
		uint32 child = _hierarchy[_node].firstChild;
		if(child & 0x80000000)
		{
			// Build a box for all triangles in the leaf
		} else {
			// Iterate through all siblings
			recomputeBVHAABoxesRec(_hierarchy, _aaBoxes, child);
			_aaBoxes[_node] = _aaBoxes[child];
			child = _hierarchy[child].escape;
			while(_hierarchy[child].parent == _node && child != 0)
			{
				recomputeBVHAABoxesRec(_hierarchy, _aaBoxes, child);
				_aaBoxes[_node] = Box(_aaBoxes[child], _aaBoxes[_node]);
				child = _hierarchy[child].escape;
			}
		}
	}

	void Chunk::recomputeBVHAABoxes()
	{
		m_aaBoxes.resize(m_hierarchy.size());
		recomputeBVHAABoxesRec(m_hierarchy.data(), m_aaBoxes.data(), 0);
	}

} // namespace bim