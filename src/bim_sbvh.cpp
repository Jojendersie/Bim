#define DEBUG
#include "bim/chunk.hpp"
#include <memory>
#include <algorithm>
#include "bim/log.hpp"

using namespace ei;

namespace bim {

	struct Bin
	{
		Box bbox;
		uint32 numStart;
		uint32 numEnd;
	};

	struct SBVBuildInfo
	{
		std::vector<Node>& hierarchy;
		std::vector<uint32>& parents;
		std::vector<UVec4>& leaves;
		std::vector<Box>& aaBoxes;
		const std::vector<Vec3>& positions;
		const std::vector<UVec3>& triangles;
		const std::vector<uint32>& materials;
		const uint numTrianglesPerLeaf;
		Vec3* centers; // triangle center position .xyz
		Vec2* heuristics; // auxiliary buffer for left/right heuristic pairs
		uint32* aux; // auxiliary work space 1
		Bin * bins;
		float rootSurface;
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


	// Get the bounding box of a clipped triangle
	static Box clippedBox(const Vec3& _a, const Vec3& _b, const Vec3& _c, int _dim, float _l, float _r)
	{
		Vec3 points[6] = {_a, _b, _a, _c, _b, _c};
		uint n = 0;
		// Clip each edge against both planes
		for(int i = 0; i < 6; i+=2)
		{
			const Vec3& a = points[i];
			const Vec3& b = points[i+1];
			if((a[_dim] >= _l || b[_dim] >= _l) && (a[_dim] <= _r || b[_dim] <= _r)) // Edge fully on one side? -> skip
			{
				Vec3 e = b - a;
				if(a[_dim] < _l) points[n] = a + e * ((_l - a[_dim]) / e[_dim]);
				else if(a[_dim] > _r) points[n] = a + e * ((_r - a[_dim]) / e[_dim]);
				else points[n] = a;
				if(b[_dim] < _l) points[n+1] = b + e * ((_l - b[_dim]) / e[_dim]);
				else if(b[_dim] > _r) points[n+1] = b + e * ((_r - b[_dim]) / e[_dim]);
				else points[n+1] = b;
				n += 2;
			}
		}

		return Box(points, n);
	}

	static Box unionBox(const Box& _a, const Box& _b)
	{
		return Box(max(_a.min, _b.min), min(_a.max, _b.max));
	}


	// Partitions all triangle into two disjunct sets. Returns the SAH cost for the
	// split and the index for the last object in the left set.
	// _min and _max are inclusive boundaries.
	static float findObjectSplit( SBVBuildInfo& _in, uint32* _triangles, uint32 _num, uint32& _outIdx, const Box& _parentBox )
	{
		// Compute lhs/rhs bounding volumes for all splits. This is done from left
		// and right adding one triangle at a time.
		UVec3 t = _in.triangles[_triangles[0]];
		Box leftBox = Box(Triangle(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z]));
		t = _in.triangles[_triangles[_num-1]];
		Box rightBox = Box(Triangle(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z]));
		_in.heuristics[0].x   = surfaceAreaHeuristic( unionBox(_parentBox, leftBox), 1 );
		_in.heuristics[_num-2].y = surfaceAreaHeuristic( unionBox(_parentBox, rightBox), 1 );
		for(uint32 i = 1; i < _num-1; ++i)
		{
			// Extend bounding boxes by one triangle
			t = _in.triangles[_triangles[i]];
			leftBox = Box(leftBox, Box(Triangle(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z])));
			t = _in.triangles[_triangles[_num-i-1]];
			rightBox = Box(rightBox, Box(Triangle(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z])));
			// The volumes are rejected in the next iteration but remember
			// the result for the split decision.
			_in.heuristics[i].x   = surfaceAreaHeuristic( unionBox(_parentBox, leftBox), i+1 );
			_in.heuristics[_num-i-2].y = surfaceAreaHeuristic( unionBox(_parentBox, rightBox), i+1 );
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

	static uint32 build( SBVBuildInfo& _in, uint32* _triangles, uint32 _num, const Box& _aab )
	{
		uint32 nodeIdx = (uint32)_in.hierarchy.size();
		_in.hierarchy.resize(nodeIdx + 1);
		_in.parents.resize(nodeIdx + 1);
		_in.hierarchy[nodeIdx].firstChild = _in.hierarchy[nodeIdx].escape = _in.parents[nodeIdx] = 0;
		_in.aaBoxes.push_back(_aab);

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
			float sah = findObjectSplit(_in, _in.aux, _num, i, _aab);
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
		float sah = findObjectSplit(_in, _in.aux, _num, i, _aab);
		if(sah < objSplitSAH)
		{
			objSplitSAH = sah;
			splitIndex = i;
			memcpy(_triangles, _in.aux, _num * 4);	// Keep best sorted result
		}//*/

		// Get the bounding boxes for the optimal object split.
		Box optLeftBox, optRightBox; // The child bounding boxes for the optimal solution
		UVec3 t = _in.triangles[_triangles[0]];
		optLeftBox = Box(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z]);
		t = _in.triangles[_triangles[_num-1]];
		optRightBox = Box(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z]);
		for(uint32 i = 1; i < splitIndex+1; ++i)
			t = _in.triangles[_triangles[i]],
			optLeftBox = Box(optLeftBox, Box(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z]));
		for(uint32 i = splitIndex+1; i < _num-1; ++i)
			t = _in.triangles[_triangles[i]],
			optRightBox = Box(optRightBox, Box(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z]));

		// Reduce number of splittings with some special conditions:
		// * only a few triangles
		// * objSplit overlap is not too bad (TODO)
		bool forceObjSplit = _num < _in.numTrianglesPerLeaf * 4
			|| (surface(unionBox(optLeftBox, optRightBox)) / _in.rootSurface <= 2e-5f);

		uint32 binSplitDim = 1000;
		float binSplitPlane = 0.0f;
		float binSplitSAH = objSplitSAH; // Only consider better solutions than the objsplit
		if(!forceObjSplit)
		for(int d = 0; d < 3; ++d)
		{
			// Try pure spatial subdivision with object duplication.
			// Use binning to compute a fixed number of reasonable split planes.
			// Each bin contains the bounding box for contained triangles, clipped at the
			// splitting planes, and two numbers to count references.
			float dimMin = _aab.min[d];
			float dimMax = _aab.max[d];
			if(approx(dimMin, dimMax)) continue; // Skip degenerated dimensions
			float binWidth = (dimMax - dimMin) / NUM_BINS;
			// Clear bin to a state without bbox
			for(int b = 0; b < NUM_BINS; ++b)
			{
				_in.bins[b].numStart = 0; _in.bins[b].numEnd = 0;
				_in.bins[b].bbox.min = Vec3(INF);
				_in.bins[b].bbox.max = Vec3(-INF);
			}
			// Insert the triangles to all relevant bins
			for(uint32 i = 0; i < _num; ++i)
			{
				UVec3 t = _in.triangles[_triangles[i]];
				float tmin = min(_in.positions[t.x][d], _in.positions[t.y][d], _in.positions[t.z][d]);
				float tmax = max(_in.positions[t.x][d], _in.positions[t.y][d], _in.positions[t.z][d]);
				int binMin = clamp(ei::floor((tmin - dimMin) / binWidth), 0, int(NUM_BINS)-1);
				int binMax = clamp(ei::floor((tmax - dimMin) / binWidth), 0, int(NUM_BINS)-1);
				// Make sure special cases are treated correct:
				// On boundary -> left bin only
				// Touches from left -> left bin only
				// Touches from right -> right bin only
				float splitPlane = dimMin + binWidth * (binMin+1);
				if(tmin >= splitPlane && tmax > splitPlane) binMin = min(binMin+1, int(NUM_BINS)-1);
				splitPlane = dimMin + binWidth * binMax;
				if(tmin <= splitPlane && tmax <= splitPlane) binMax = max(binMin, binMax-1);
				eiAssert(binMin >= 0 && binMin < NUM_BINS, "Invalid bin index.");
				eiAssert(binMax >= 0 && binMax < NUM_BINS, "Invalid bin index.");

				_in.bins[binMin].numStart++;
				_in.bins[binMax].numEnd++;
				for(int b = binMin; b <= binMax; ++b)
					_in.bins[b].bbox = Box(_in.bins[b].bbox, clippedBox(_in.positions[t.x], _in.positions[t.y], _in.positions[t.z], d,
						dimMin + binWidth * b,
						(b == NUM_BINS-1) ? dimMax : dimMin + binWidth * (b+1))); // Numerical problems force us to use the real boundary of the last bucket instead of the computed one
			}
			// The boxes are only restricted in binning dimension, make sure to not increase
			// the box through triangles which are referenced from different nodes.
			for(int b = 0; b < NUM_BINS; ++b)
				_in.bins[b].bbox = unionBox(_aab, _in.bins[b].bbox);
			// TODO: remove double references like in the paper
			// Find cost for spatial splitting.
			Box leftBox = _in.bins[0].bbox;
			Box rightBox = _in.bins[NUM_BINS-1].bbox;
			uint32 numLeft = _in.bins[0].numStart;
			uint32 numRight = _in.bins[NUM_BINS-1].numEnd;
			_in.heuristics[0].x			 = surfaceAreaHeuristic( leftBox, numLeft );
			_in.heuristics[NUM_BINS-2].y = surfaceAreaHeuristic( rightBox, numRight );
			// Use aux to track the total number of references through the splitting
			for(uint i = 0; i < NUM_BINS; ++i) _in.aux[i] = 0;
			_in.aux[0] = numLeft;
			_in.aux[NUM_BINS-2] = numRight;
			for(uint i = 1; i < NUM_BINS-1; ++i)
			{
				leftBox = Box(leftBox, _in.bins[i].bbox);
				rightBox = Box(rightBox, _in.bins[NUM_BINS-i-1].bbox);
				numLeft += _in.bins[i].numStart;
				numRight += _in.bins[NUM_BINS-i-1].numEnd;
				_in.heuristics[i].x = numLeft < _num ? surfaceAreaHeuristic( leftBox, numLeft ) : INF;
				_in.heuristics[NUM_BINS-i-2].y = numRight < _num ? surfaceAreaHeuristic( rightBox, numRight ) : INF;
				_in.aux[i] += numLeft;
				_in.aux[NUM_BINS-i-2] += numRight;
			}
			// Find the minimum for binned splitting
			uint splitBin = 0;
			for(uint32 i = 0; i < NUM_BINS-1; ++i)
			{
				if(sum(_in.heuristics[i]) < binSplitSAH
					&& _in.aux[i]*3 < _num*4 ) // Do not allow more than 33% reference duplication in one step
				{
					binSplitSAH = sum(_in.heuristics[i]);
					binSplitDim = d;
					splitBin = i;
				}
			}

			// Get the bounding boxes for the optimal split (intermediate information
			// are discarded after each dimension -> do it inside the loop).
			if(binSplitDim == d)
			{
				binSplitPlane = dimMin + binWidth * (splitBin+1);
				optLeftBox = _in.bins[0].bbox;
				optRightBox = _in.bins[NUM_BINS-1].bbox;
				for(uint i = 1; i < splitBin+1; ++i)
					optLeftBox = Box(optLeftBox, _in.bins[i].bbox);
				for(uint i = splitBin+1; i < NUM_BINS-1; ++i)
					optRightBox = Box(optRightBox, _in.bins[i].bbox);
				eiAssert(optLeftBox.min != optRightBox.min || optLeftBox.max != optRightBox.max, "Spatial split must divide the space.");
			}
		}

		bool useObjSplit = objSplitSAH <= binSplitSAH;

		// Build a new index set for the left side
		std::unique_ptr<uint32[]> tmpIndexSet(new uint32[_num]);
		uint32 n = 0;
		if(useObjSplit)
		{
			n = splitIndex+1;
			memcpy(tmpIndexSet.get(), _triangles, n * 4);
		} else {
			// Get all triangles which start before the split plane
			n = 0;
			for(uint32 i = 0; i < _num; ++i)
			{
				UVec3 t = _in.triangles[_triangles[i]];
				float tmin = min(_in.positions[t.x][binSplitDim], _in.positions[t.y][binSplitDim], _in.positions[t.z][binSplitDim]);
				float tmax = max(_in.positions[t.x][binSplitDim], _in.positions[t.y][binSplitDim], _in.positions[t.z][binSplitDim]);
				if(tmin < binSplitPlane || tmax <= binSplitPlane) // Must start truly in bucket or be on boundary
					tmpIndexSet[n++] = _triangles[i];
			}
		}
		// Set left and right into firstChild and escape. This is corrected later in
		// remapNodePointers().
		_in.hierarchy[nodeIdx].firstChild = build( _in, tmpIndexSet.get(), n, optLeftBox );
		// Build index set for the right side
		if(useObjSplit)
		{
			n = _num - splitIndex - 1;
			memcpy(tmpIndexSet.get(), _triangles + splitIndex + 1, n * 4);
		} else {
			// Get all triangles which end after the split plane
			n = 0;
			for(uint32 i = 0; i < _num; ++i)
			{
				UVec3 t = _in.triangles[_triangles[i]];
				float tmax = max(_in.positions[t.x][binSplitDim], _in.positions[t.y][binSplitDim], _in.positions[t.z][binSplitDim]);
				if(tmax > binSplitPlane)
					tmpIndexSet[n++] = _triangles[i];
			}
		}
		_in.hierarchy[nodeIdx].escape = build( _in, tmpIndexSet.get(), n, optRightBox );

		return nodeIdx;
	}

	void Chunk::buildBVH_SBVH()
	{
		Vec3 a(0.0f, 0.0f, 0.0f);
		Vec3 b(1.0f, 0.0f, 0.0f);
		Vec3 c(0.0f, 1.0f, 0.0f);
		Box box(Vec3(0.5f, -1.0f, -1.0f), Vec3(1.5f, 1.0f, 1.0f));
		Box res = clippedBox(a, b, c, 0, 0.5f, 0.75f);

		uint32 n = getNumTriangles();
		std::unique_ptr<Vec3[]> centers(new Vec3[n]);
		std::unique_ptr<Vec2[]> heuristics(new Vec2[max(NUM_BINS,n-1)]); // n-1 split positions
		std::unique_ptr<uint32[]> auxA(new uint32[max(NUM_BINS,n)]);
		std::unique_ptr<uint32[]> indices(new uint32[n]);
		std::unique_ptr<Bin[]> bins(new Bin[NUM_BINS]);

		// Initialize unsorted and centers
		for( uint32 i = 0; i < n; ++i )
		{
			UVec3 t = getTriangles()[i];
			centers[i] = (m_positions[t.x] + m_positions[t.y] + m_positions[t.z]) / 3.0f;
			indices[i] = i;
		}

		m_hierarchy.reserve(n*2);
		m_hierarchyParents.reserve(n*2);
		m_aaBoxes.reserve(n*2);
		m_hierarchyLeaves.reserve(n);
		SBVBuildInfo input = {m_hierarchy, m_hierarchyParents, m_hierarchyLeaves,
			m_aaBoxes, m_positions, m_triangles, m_triangleMaterials,
			m_numTrianglesPerLeaf, centers.get(), heuristics.get(),
			auxA.get(), bins.get(), surface(m_boundingBox)};
		build(input, indices.get(), n, m_boundingBox);
		m_properties = Property::Val(m_properties | Property::AABOX_BVH);

		bim::sendMessage(MessageType::INFO, "SBVH split produced ", m_hierarchyLeaves.size() / float(n) * 100.0f, " % references.");
	}

} // namespace bim