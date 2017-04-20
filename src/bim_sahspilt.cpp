#include "bim/chunk.hpp"
#include <memory>
#include <algorithm>

using namespace ei;

namespace bim {

	struct SAHBuildInfo
	{
		std::vector<Node>& hierarchy;
		std::vector<uint32>& parents;
		std::vector<UVec4>& leaves;
		const std::vector<Vec3>& positions;
		const std::vector<UVec3>& triangles;
		const std::vector<uint32>& materials;
		const uint numTrianglesPerLeaf;
		uint32* sortedIDs;
		Vec4* centers; // position .xyz and projection in .w
	};

	static float surfaceAreaHeuristic(const Box& _bv, int _num)
	{
		return surface(_bv) * _num;
	}

	static uint32 build( SAHBuildInfo& _in, uint32 _min, uint32 _max )
	{
		uint32 nodeIdx = (uint32)_in.hierarchy.size();
		_in.hierarchy.resize(nodeIdx + 1);
		_in.parents.resize(nodeIdx + 1);
		_in.hierarchy[nodeIdx].firstChild = _in.hierarchy[nodeIdx].escape = _in.parents[nodeIdx] = 0;

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
				*(trianglesPtr++) = UVec4( _in.triangles[_in.sortedIDs[i]], _in.materials.empty() ? 0 : _in.materials[_in.sortedIDs[i]] );
			for( uint i = 0; i < _in.numTrianglesPerLeaf - (_max - _min + 1); ++i )
				*(trianglesPtr++) = UVec4(0); // Invalid triangle per convention

			// Let new inner node pointing to this leaf
			_in.hierarchy[nodeIdx].firstChild = 0x80000000 | (uint32)(leafIdx / _in.numTrianglesPerLeaf);

			return nodeIdx;
		}

		// Compute a covariance matrix for the current set of center points (2 passes)
		Vec3 mean = _in.centers[_in.sortedIDs[_min]].subcol<0,3>();
		for( uint32 i = _min+1; i <= _max; ++i )
			mean += _in.centers[_in.sortedIDs[i]].subcol<0,3>();
		mean /= _max - _min + 1;
		Mat3x3 cov(0.0f);	// Variances
		for( uint32 i = _min; i <= _max; ++i )
		{
			Vec3 e = _in.centers[_in.sortedIDs[i]].subcol<0,3>() - mean;
			cov += Mat3x3(e.x*e.x, e.x*e.y, e.x*e.z,
				e.x*e.y, e.y*e.y, e.y*e.z,
				e.x*e.z, e.y*e.z, e.z*e.z);
		}
		cov /= _max - _min; // div n-1 for unbiased variance

		// Get the largest eigenvalues' vector, this is the direction with the largest
		// Geometry deviation. Split in this direction.
		Mat3x3 Q;
		Vec3 λ;
		decomposeQl( cov, Q, λ );
		Vec3 splitDir;
		eiAssert((λ >= -1e6), "Only positive eigenvalues expected!");
		if(λ.x > λ.y && λ.x > λ.z) splitDir = transpose(Q(0));
		else if(λ.y > λ.z) splitDir = transpose(Q(1));
		else splitDir = transpose(Q(2));

		// Sort current range in the projected dimension
		for( uint32 i = _min; i <= _max; ++i )
			_in.centers[_in.sortedIDs[i]].w = dot(_in.centers[_in.sortedIDs[i]].subcol<0,3>(), splitDir);
		std::sort( _in.sortedIDs + _min, _in.sortedIDs + _max + 1,
			[&](const uint32 _lhs, const uint32 _rhs) { return _in.centers[_lhs].w < _in.centers[_rhs].w; }
		);

		// 2. Sweep to find the optimal split position

		// Compute lhs/rhs bounding volumes for all splits. This is done from left
		// and right adding one triangle at a time.
		std::unique_ptr<Vec2[]> heuristics(new Vec2[_max - _min]);
		UVec3 t = _in.triangles[_in.sortedIDs[_min]];
		Box leftBox = Box(Triangle(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z]));
		t = _in.triangles[_in.sortedIDs[_max]];
		Box rightBox = Box(Triangle(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z]));
		heuristics[0].x   = surfaceAreaHeuristic( leftBox, 1 );
		heuristics[_max-_min-1].y = surfaceAreaHeuristic( rightBox, 1 );
		for(uint32 i = _min + 1; i < _max; ++i)
		{
			// Extend bounding boxes by one triangle
			t = _in.triangles[_in.sortedIDs[i]];
			leftBox = Box(leftBox, Box(Triangle(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z])));
			t = _in.triangles[_in.sortedIDs[_max-i+_min]];
			rightBox = Box(rightBox, Box(Triangle(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z])));
			// The volumes are rejected in the next iteration but remember
			// the result for the split decision.
			heuristics[i-_min].x   = surfaceAreaHeuristic( leftBox, i-_min+1 );
			heuristics[_max-i-1].y = surfaceAreaHeuristic( rightBox, i-_min+1 );
		}

		// Find the minimum for the current dimension
		uint32 m = 0;
		float minCost = std::numeric_limits<float>::infinity();
		for(uint32 i = 0; i < (_max-_min); ++i)
		{
			if(sum(heuristics[i]) < minCost)
			{
				minCost = sum(heuristics[i]);
				m = i + _min;
			}
		}

		// Set left and right into firstChild and escape. This is corrected later in
		// remapNodePointers().
		_in.hierarchy[nodeIdx].firstChild = build( _in, _min, m );
		_in.hierarchy[nodeIdx].escape = build( _in, m+1, _max );

		return nodeIdx;
	}
	
	void Chunk::buildBVH_SAHsplit()
	{
		uint32 n = getNumTriangles();
		std::unique_ptr<Vec4[]> centers(new Vec4[n]);
		std::unique_ptr<uint32[]> ids(new uint32[n]);
		for( uint32 i = 0; i < n; ++i )
		{
			ids[i] = i;
			UVec3 t = getTriangles()[i];
			centers[i] = Vec4((m_positions[t.x] + m_positions[t.y] + m_positions[t.z]) / 3.0f, 0.0f);
		}

		m_hierarchy.reserve(n*2);
		m_hierarchyParents.reserve(n*2);
		SAHBuildInfo input = {m_hierarchy, m_hierarchyParents, m_hierarchyLeaves, m_positions,
			m_triangles, m_triangleMaterials, m_numTrianglesPerLeaf,
			ids.get(), centers.get()};
		build(input, 0, n-1);
	}

} // namespace bim