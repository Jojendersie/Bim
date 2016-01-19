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

} // namespace bim