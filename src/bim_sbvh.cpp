#include "bim/chunk.hpp"
#include <memory>
#include <algorithm>

using namespace ei;

namespace bim {

	struct SBVBuildInfo
	{
		std::vector<Node>& hierarchy;
		std::vector<uint32>& parents;
		std::vector<UVec4>& leaves;
		const std::vector<Vec3>& positions;
		const std::vector<UVec3>& triangles;
		const std::vector<uint32>& materials;
		const uint numTrianglesPerLeaf;
		Vec3* centers; // triangle center position .xyz
		Vec2* heuristics; // auxiliary buffer for left/right heuristic pairs
		uint32* aux; // auxiliary work space 1
	};

	const uint NUM_BINS = 256;

	static float surfaceAreaHeuristic(const Box& _bv, int _num)
	{
		return surface(_bv) * _num;
	}

	// Two sources to derive the z-order comparator
	// (floats - unused) http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.150.9547&rep=rep1&type=pdf
	// (ints - the below one uses this int-algorithm on floats) http://dl.acm.org/citation.cfm?id=545444
	// http://www.forceflow.be/2013/10/07/morton-encodingdecoding-through-bit-interleaving-implementations/ Computing morton codes
	static uint64 partby2(uint16 _x)
	{
		uint64 r = _x;
		r = (r | (r << 16)) & 0x0000ff0000ff;
		r = (r | (r <<  8)) & 0x00f00f00f00f;
		r = (r | (r <<  4)) & 0x0c30c30c30c3;
		r = (r | (r <<  2)) & 0x249249249249;
		return r;
	}
	// Insert two 0 bits before each bit. This transforms a 16 bit number xxxxxxxx into a 48 bit number 00x00x00x00x00x00x00x00x
	static uint64 morton(uint16 _a, uint16 _b, uint16 _c)
	{
		return partby2(_a) | (partby2(_b) << 1) | (partby2(_c) << 2);
	}

	// Usefull sources for hilber curve implementations:
	// http://www.tiac.net/~sw/2008/10/Hilbert/moore/ Non recursive c-code
	// http://stackoverflow.com/questions/8459562/hilbert-sort-by-divide-and-conquer-algorithm Idea with inverse gray code
	static uint64 binaryToGray(uint64 num)
	{
		return num ^ (num >> 1);
	}
	static uint64 grayToBinary(uint64 num)
	{
		num = num ^ (num >> 32);
		num = num ^ (num >> 16);
		num = num ^ (num >> 8);
		num = num ^ (num >> 4);
		num = num ^ (num >> 2);
		num = num ^ (num >> 1);
		return num;
	}
	static bool hilbertcurvecmp(const Vec3& _a, const Vec3& _b)
	{
		// Get (positive) integer vectors
		UVec3 a = UVec3(_a * 4096.0f);
		UVec3 b = UVec3(_b * 4096.0f);
		// Interleave packages of 16 bit, convert to inverse Gray code and compare.
		uint64 codeA = grayToBinary(morton(a.x >> 16, a.y >> 16, a.z >> 16));
		uint64 codeB = grayToBinary(morton(b.x >> 16, b.y >> 16, b.z >> 16));
		// If they are equal take the next 16 less significant bit.
		if(codeA == codeB) {
			codeA = grayToBinary(morton(a.x & 0xffff, a.y & 0xffff, a.z & 0xffff));
			codeB = grayToBinary(morton(b.x & 0xffff, b.y & 0xffff, b.z & 0xffff));
		}
		return codeA < codeB;
	}

	// Partitions all triangle into two disjunct sets. Returns the SAH cost for the
	// split and the index for the last object in the left set.
	// _min and _max are inclusive boundaries.
	static float findObjectSplit( SBVBuildInfo& _in, uint32* _triangles, uint32 _num, uint32& _outIdx )
	{
		// Compute lhs/rhs bounding volumes for all splits. This is done from left
		// and right adding one triangle at a time.
		UVec3 t = _in.triangles[_triangles[0]];
		Box leftBox = Box(Triangle(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z]));
		t = _in.triangles[_triangles[_num-1]];
		Box rightBox = Box(Triangle(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z]));
		_in.heuristics[0].x   = surfaceAreaHeuristic( leftBox, 1 );
		_in.heuristics[_num-2].y = surfaceAreaHeuristic( rightBox, 1 );
		for(uint32 i = 1; i < _num-1; ++i)
		{
			// Extend bounding boxes by one triangle
			t = _in.triangles[_triangles[i]];
			leftBox = Box(leftBox, Box(Triangle(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z])));
			t = _in.triangles[_triangles[_num-i-1]];
			rightBox = Box(rightBox, Box(Triangle(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z])));
			// The volumes are rejected in the next iteration but remember
			// the result for the split decision.
			_in.heuristics[i].x   = surfaceAreaHeuristic( leftBox, i+1 );
			_in.heuristics[_num-i-2].y = surfaceAreaHeuristic( rightBox, i+1 );
		}

		// Find the minimum for the current dimension
		_outIdx = 0;
		float minCost = std::numeric_limits<float>::infinity();
		for(uint32 i = 0; i < _num-1; ++i)
		{
			if(sum(_in.heuristics[i]) < minCost)
			{
				minCost = sum(_in.heuristics[i]);
				_outIdx = i;
			}
		}
		return minCost;
	}

	static uint32 build( SBVBuildInfo& _in, uint32* _triangles, uint32 _num )
	{
		uint32 nodeIdx = (uint32)_in.hierarchy.size();
		_in.hierarchy.resize(nodeIdx + 1);
		_in.parents.resize(nodeIdx + 1);
		_in.hierarchy[nodeIdx].firstChild = _in.hierarchy[nodeIdx].escape = _in.parents[nodeIdx] = 0;

		// Create a leaf if less than NUM_PRIMITIVES elements remain.
		eiAssert(_num > 0, "Node without triangles!");
		if( _num <= _in.numTrianglesPerLeaf )
		{
			// Allocate a new leaf
			size_t leafIdx = _in.leaves.size();
			_in.leaves.resize(_in.leaves.size() + _in.numTrianglesPerLeaf);
			// Fill it
			UVec4* trianglesPtr = &_in.leaves[leafIdx];
			for( uint i = 0; i < _num; ++i )
				*(trianglesPtr++) = UVec4( _in.triangles[_triangles[i]], _in.materials.empty() ? 0 : _in.materials[_triangles[i]] );
			for( uint i = 0; i < _in.numTrianglesPerLeaf - _num; ++i )
				*(trianglesPtr++) = UVec4(0); // Invalid triangle per convention

			// Let the new node pointing to this leaf
			_in.hierarchy[nodeIdx].firstChild = 0x80000000 | (uint32)(leafIdx / _in.numTrianglesPerLeaf);

			return nodeIdx;
		}

		// Find SAH object split candidate.
		uint32 splitIndex = 0; // Last triangle of left set
		float objSplitSAH = std::numeric_limits<float>::infinity();
		memcpy(_in.aux, _triangles, _num * 4);
		for(int d = 0; d < 3; ++d)
		{
			std::sort( _in.aux, _in.aux + _num,
				[&](const uint32 _lhs, const uint32 _rhs) { return _in.centers[_lhs][d] < _in.centers[_rhs][d]; }
			);

			uint32 i;
			float sah = findObjectSplit(_in, _in.aux, _num, i);
			if(sah < objSplitSAH)
			{
				objSplitSAH = sah;
				splitIndex = i;
				memcpy(_triangles, _in.aux, _num * 4);	// Keep best sorted result
			}
		}
		// Try again with Hilbert curve instead of the main axis.
		/*std::sort( _in.aux, _in.aux + _num,
			[&](const uint32 _lhs, const uint32 _rhs) { return hilbertcurvecmp(_in.centers[_lhs], _in.centers[_rhs]); }
		);
		uint32 i;
		float sah = findObjectSplit(_in, _in.aux, _num, i);
		if(sah < objSplitSAH)
		{
			objSplitSAH = sah;
			splitIndex = i;
			memcpy(_triangles, _in.aux, _num * 4);	// Keep best sorted result
		}//*/

		// Build a new index set for the left side
		std::unique_ptr<uint32[]> tmpIndexSet(new uint32[_num]);
		memcpy(tmpIndexSet.get(), _triangles, (splitIndex+1) * 4);
		// Set left and right into firstChild and escape. This is corrected later in
		// remapNodePointers().
		_in.hierarchy[nodeIdx].firstChild = build( _in, tmpIndexSet.get(), splitIndex+1 );
		// Bulid index set for the right side
		memcpy(tmpIndexSet.get(), _triangles + splitIndex + 1, (_num - splitIndex - 1) * 4);
		_in.hierarchy[nodeIdx].escape = build( _in, tmpIndexSet.get(), _num - splitIndex - 1 );

		return nodeIdx;
	}

	void Chunk::buildBVH_SBVH()
	{
		uint32 n = getNumTriangles();
		std::unique_ptr<Vec3[]> centers(new Vec3[n]);
		std::unique_ptr<Vec2[]> heuristics(new Vec2[ei::max(NUM_BINS,n-1)]); // n-1 split positions
		std::unique_ptr<uint32[]> auxA(new uint32[n]);
		std::unique_ptr<uint32[]> indices(new uint32[n]);

		// Initialize unsorted and centers
		for( uint32 i = 0; i < n; ++i )
		{
			UVec3 t = getTriangles()[i];
			centers[i] = (m_positions[t.x] + m_positions[t.y] + m_positions[t.z]) / 3.0f;
			indices[i] = i;
		}

		m_hierarchy.reserve(n*2);
		m_hierarchyParents.reserve(n*2);
		m_hierarchyLeaves.reserve(n);
		SBVBuildInfo input = {m_hierarchy, m_hierarchyParents, m_hierarchyLeaves,
			m_positions, m_triangles, m_triangleMaterials, m_numTrianglesPerLeaf,
			centers.get(), heuristics.get(), auxA.get()};
		build(input, indices.get(), n);
	}

} // namespace bim