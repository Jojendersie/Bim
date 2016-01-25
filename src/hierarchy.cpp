#include "chunk.hpp"
#include <cn/chinoise.hpp>

using namespace ei;

namespace bim {

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
				// There are invalid triangles if a node is not filled
				if(_leaves[leafIdx+i].x == _leaves[leafIdx+i].y)
					break;
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
		m_properties = Property::Val(m_properties | Property::AABOX_BVH);
	}



	const int NORMAL_SAMPLES = 1000;

	// Linear interpolable values of the symmetric 3x3 matrix.
	// This uncompressed form (contrary to Chunk::SGGX) serves as
	//		intermediate result.
	struct TmpSGGX
	{
		float xx, xy, xz, yy, yz, zz;
	};

	// _leaf Offseted pointer to the current leaf (not the entire array)
	static TmpSGGX computeLeafSGGXBase(const Vec3* _positions, const Vec3* _normals, const UVec4* _leaf, uint _numTrianglesPerLeaf)
	{
		// Load triangle geometry
		Triangle pos[16]; // TODO: DANGEROUS if _numLeaves > 16, but using local stack is much faster
		Vec3 nrm[16 * 3];
		int n = 0; // Count real number of triangles
		for( ; n < (int)_numTrianglesPerLeaf; ++n)
		{
			if(_leaf[n].x == _leaf[n].y) break;
			pos[n] = Triangle(_positions[_leaf[n].x], _positions[_leaf[n].y], _positions[_leaf[n].z]);
			nrm[n*3    ] = _normals[_leaf[n].x];
			nrm[n*3 + 1] = _normals[_leaf[n].y];
			nrm[n*3 + 2] = _normals[_leaf[n].z];
		}

		// Sample random normals from all triangles and compute spherical distribution variances.
		Mat3x3 E(0.0f); // Expectations, later covariance matrix
		float area = surface(pos[0]);
		for(int i = 1; i < n; ++i) area += surface(pos[i]);
		for(int i = 0; i < n; ++i)
		{
			cn::HaltonRng haltonSeq(2);
			// Get percentage of the total number of samples
			int nsamples = int(NORMAL_SAMPLES * surface(pos[i]) / area);
			for(int j = 0; j < nsamples; ++j)
			{
				Vec3 bary = cn::barycentric(haltonSeq);
				Vec3 normal = normalize(nrm[i*3] * bary.x + nrm[i*3+1] * bary.y + nrm[i*3+1] * bary.z);
				// Update covariance matrix
				E.m00 += normal.x * normal.x;
				E.m01 += normal.x * normal.y;
				E.m02 += normal.x * normal.z;
				E.m11 += normal.y * normal.y;
				E.m12 += normal.y * normal.z;
				E.m22 += normal.z * normal.z;
			}
		}
		// Copy symmetric part of the matrix. Do not need to normalize by sample
		// count because the scale (eigenvalues) are not of interest.
		E.m10 = E.m01; E.m20 = E.m02; E.m21 = E.m12;
		// Get eigenvectors they are the same as for the SGGX base
		Mat3x3 Q; Vec3 λ;
		decomposeQl(E, Q, λ, true);

		// Compute projected areas in the directions of eigenvectors using the same
		// distribution as before.
		λ = Vec3(0.0f);
		int nTotalSamples = 0;
		for(int i = 0; i < n; ++i)
		{
			cn::HaltonRng haltonSeq(2);
			// Get percentage of the total number of samples
			int nsamples = int(NORMAL_SAMPLES * surface(pos[i]) / area);
			nTotalSamples += nsamples;
			for(int j = 0; j < nsamples; ++j)
			{
				Vec3 bary = cn::barycentric(haltonSeq);
				Vec3 normal = normalize(nrm[i*3] * bary.x + nrm[i*3+1] * bary.y + nrm[i*3+1] * bary.z);
				λ[0] += abs(Q(0) * normal);
				λ[1] += abs(Q(1) * normal);
				λ[2] += abs(Q(2) * normal);
			}
		}
		λ /= nTotalSamples;// TODO: /4π ?
		E = transpose(Q) * diag(λ) * Q;
		TmpSGGX s;
		s.xx = E.m00; s.xy = E.m01; s.xz = E.m02;
		s.yy = E.m11; s.yz = E.m12;
		s.zz = E.m22;
		eiAssert(s.xx >= 0.0f && s.xx <= 1.0f, "Value Sxx outside expected range!");
		eiAssert(s.yy >= 0.0f && s.yy <= 1.0f, "Value Syy outside expected range!");
		eiAssert(s.zz >= 0.0f && s.zz <= 1.0f, "Value Szz outside expected range!");
		eiAssert(s.xy >= -1.0f && s.xy <= 1.0f, "Value Sxy outside expected range!");
		eiAssert(s.xz >= -1.0f && s.xz <= 1.0f, "Value Sxz outside expected range!");
		eiAssert(s.yz >= -1.0f && s.yz <= 1.0f, "Value Syz outside expected range!");
		return s;
	}

	static TmpSGGX computeBVHSGGXApproximationsRec(const Vec3* _positions, const Vec3* _normals, const Node* _hierarchy, uint32 _node, const UVec4* _leaves, const Box* _aaBoxes, uint _numTrianglesPerLeaf, std::vector<SGGX>& _output)
	{
		TmpSGGX s;
		// End of recursion (inner node which points to one leaf)
		uint32 child = _hierarchy[_node].firstChild;
		if(child & 0x80000000)
		{
			s = computeLeafSGGXBase(_positions, _normals, _leaves + (child & 0x7fffffff) * _numTrianglesPerLeaf, _numTrianglesPerLeaf);
		} else {
			s = computeBVHSGGXApproximationsRec(_positions, _normals, _hierarchy, child, _leaves, _aaBoxes, _numTrianglesPerLeaf, _output);
			child = _hierarchy[child].escape;
			float lw = surface(_aaBoxes[child]);
			eiAssert(_hierarchy[child].parent == _node && child!=0 && _hierarchy[_hierarchy[child].escape].parent != _node,
				"Expected binary tree!");
			TmpSGGX s2 = computeBVHSGGXApproximationsRec(_positions, _normals, _hierarchy, child, _leaves, _aaBoxes, _numTrianglesPerLeaf, _output);
			// Weight depending on subtree bounding volume sizes
			float rw = surface(_aaBoxes[child]);
			float wsum = lw + rw;
			lw /= wsum; rw /= wsum;
			s.xx = s.xx * lw + s2.xx * rw;
			s.xy = s.xy * lw + s2.xy * rw;
			s.xz = s.xz * lw + s2.xz * rw;
			s.yy = s.yy * lw + s2.yy * rw;
			s.yz = s.yz * lw + s2.yz * rw;
			s.zz = s.zz * lw + s2.zz * rw;
		}
		// Store compressed form
		_output[_node].σ = Vec<uint16, 3>(sqrt(Vec3(s.xx, s.yy, s.zz)) * 65535.0f);
		_output[_node].r = Vec<uint16, 3>(Vec3(s.xy, s.xz, s.yz) / sqrt(Vec3(s.xx*s.yy, s.xx*s.zz, s.yy*s.zz)) * 32767.0f + 32767.0f);
		return s;
	}

	void Chunk::computeBVHSGGXApproximations()
	{
		swap(m_nodeNDFs, std::vector<SGGX>(getNumNodes()));
		computeBVHSGGXApproximationsRec(m_positions.data(),
			m_normals.data(),
			m_hierarchy.data(),
			0,
			m_hierarchyLeaves.data(),
			m_aaBoxes.data(),
			m_numTrianglesPerLeaf,
			m_nodeNDFs);

		m_properties = Property::Val(m_properties | Property::NDF_SGGX);
	}



	uint Chunk::remapNodePointers(uint32 _this, uint32 _parent, uint32 _escape)
	{
		// Keep firstChild, because firstChild == left in any case.
		uint32 left = m_hierarchy[_this].firstChild;
		uint32 right = m_hierarchy[_this].escape;
		m_hierarchy[_this].parent = _parent;
		m_hierarchy[_this].escape = _escape;
		if(!(left & 0x80000000)) // If not a leaf-child
		{
			uint l = remapNodePointers(left, _this, right);
			uint r = remapNodePointers(right, _this, _escape);
			return max(l, r) + 1;
		}
		return 1;
	}

} // namespace bim