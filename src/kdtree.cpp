#include "chunk.hpp"
#include <memory>
#include <algorithm>

using namespace ei;

namespace bim {

	struct KDTreeBuildInfo
	{
		std::vector<Node>& hierarchy;
		std::vector<UVec4>& leaves;
		std::vector<UVec3>& triangles;
		std::vector<uint32>& materials;
		uint numTrianglesPerLeaf;
		const std::unique_ptr<uint32[]>* sorted;
		Vec3* centers;
	};

	static void split( uint32* _list, const Vec3* _centers, uint32 _size, int _splitDim, float _splitPlane )
	{
		// Make a temporary copy
		std::vector<uint32> tmp( _size );

		// The first half should have a size of (_size + 1) / 2 which is half of
		// the elements and in case of a odd number the additional element goes to
		// the left.
		uint32 rightOff = (_size + 1) / 2;
		uint32 l = 0, r = rightOff;
		for( uint32 i = 0; i < _size; ++i )
		{
			if( _centers[_list[i]][_splitDim] <= _splitPlane )
				tmp[l++] = _list[i];
			else tmp[r++] = _list[i];
		}
		eiAssert( l == rightOff, "Offset of right half was wrong!" );
		eiAssert( l+r-rightOff == _size, "Inconsistent split - not all elements were copied!" );
		// Copy back
		memcpy( _list, &tmp[0], _size * sizeof(uint32) );
	}

	static uint32 build( KDTreeBuildInfo& _in, uint32 _min, uint32 _max )
	{
		uint32 nodeIdx = (uint32)_in.hierarchy.size();
		_in.hierarchy.resize(nodeIdx + 1);
		_in.hierarchy[nodeIdx].firstChild = _in.hierarchy[nodeIdx].escape = _in.hierarchy[nodeIdx].parent = 0;

		// Create a leaf if less than NUM_PRIMITIVES elements remain.
		eiAssert(_min <= _max, "Node without triangles!");
		if( (_max - _min) < _in.numTrianglesPerLeaf )
		{
			// Allocate a new leaf
			size_t leafIdx = _in.leaves.size();
			_in.leaves.resize(_in.leaves.size() + _in.numTrianglesPerLeaf);
			// Fill it
			UVec4* trianglesPtr = &_in.leaves[leafIdx];
			for( uint i = _min; i <= _max; ++i )
				*(trianglesPtr++) = UVec4( _in.triangles[_in.sorted[0][i]], _in.materials.empty() ? 0 : _in.materials[_in.sorted[0][i]] );
			for( uint i = 0; i < _in.numTrianglesPerLeaf - (_max - _min + 1); ++i )
				*(trianglesPtr++) = UVec4(0); // Invalid triangle per convention

			// Let the new node pointing to this leaf
			_in.hierarchy[nodeIdx].firstChild = 0x80000000 | (uint32)(leafIdx / _in.numTrianglesPerLeaf);

			return nodeIdx;
		}

		// Find dimension with largest extension
		Box bb;
		bb.min = Vec3( _in.centers[_in.sorted[0][_min]].x,
					   _in.centers[_in.sorted[1][_min]].y,
					   _in.centers[_in.sorted[2][_min]].z );
		bb.max = Vec3( _in.centers[_in.sorted[0][_max]].x,
					   _in.centers[_in.sorted[1][_max]].y,
					   _in.centers[_in.sorted[2][_max]].z );
		Vec3 w = bb.max - bb.min;
		int dim = 0;
		if( w[1] > w[0] && w[1] > w[2] ) dim = 1;
		if( w[2] > w[0] && w[2] > w[1] ) dim = 2;
		int codim1 = (dim + 1) % 3;
		int codim2 = (dim + 2) % 3;

		// Split at median
		uint32 m = ( _min + _max ) / 2;
		// If there are many elements with the same coordinate change their
		// positions temporary to something greater until split is done. Then
		// reset them back to the median value.
		uint32 numChanged = 0;
		float splitPlane = _in.centers[_in.sorted[dim][m]][dim];
		while( m+numChanged < _max && _in.centers[_in.sorted[dim][m+1+numChanged]][dim] == splitPlane )
		{
			// The added amount is irrelevant because it is reset after split anyway.
			_in.centers[_in.sorted[dim][m+1+numChanged]][dim] += 1.0f;
			++numChanged;
		}
		// The split requires to reorder the two other dimension arrays
		split( &_in.sorted[codim1][_min], _in.centers, _max - _min + 1, dim, splitPlane );
		split( &_in.sorted[codim2][_min], _in.centers, _max - _min + 1, dim, splitPlane );
		// Reset dimension values
		for( uint32 i = m+1; i < m+1+numChanged; ++i )
			_in.centers[_in.sorted[dim][i]][dim] = splitPlane;

		// Set left and right into firstChild and escape. This is corrected later in
		// remapNodePointers().
		_in.hierarchy[nodeIdx].firstChild = build( _in, _min, m );
		_in.hierarchy[nodeIdx].escape = build( _in, m+1, _max );

		return nodeIdx;
	}

	void Chunk::buildBVH_kdtree()
	{
		uint32 n = getNumTriangles();
		std::unique_ptr<Vec3[]> centers(new Vec3[n]);
		// Create 3 sorted arrays for the dimensions
		std::unique_ptr<uint32[]> sorted[3] = {
			std::unique_ptr<uint32[]>(new uint32[n]),
			std::unique_ptr<uint32[]>(new uint32[n]),
			std::unique_ptr<uint32[]>(new uint32[n])
		};

		// Initialize unsorted and centers
		for( uint32 i = 0; i < n; ++i )
		{
			sorted[0][i] = i;
			sorted[1][i] = i;
			sorted[2][i] = i;
			UVec3 t = getTriangles()[i];
			centers[i] = (m_positions[t.x] + m_positions[t.y] + m_positions[t.z]) / 3.0f;
		}

		// Sort according to center
		std::sort( &sorted[0][0], &sorted[0][n],
			[&centers](const uint32 _lhs, const uint32 _rhs) { return centers[_lhs].x < centers[_rhs].x; }
		);
		std::sort( &sorted[1][0], &sorted[1][n],
			[&centers](const uint32 _lhs, const uint32 _rhs) { return centers[_lhs].y < centers[_rhs].y; }
		);
		std::sort( &sorted[2][0], &sorted[2][n],
			[&centers](const uint32 _lhs, const uint32 _rhs) { return centers[_lhs].z < centers[_rhs].z; }
		);

		KDTreeBuildInfo input = {m_hierarchy, m_hierarchyLeaves,
			m_triangles, m_triangleMaterials, m_numTrianglesPerLeaf,
			sorted, centers.get()};
		build(input, 0, n-1);
	}

} // namespace bim