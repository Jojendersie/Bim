#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

namespace bim {

	/// The hash grid maps keys with a position in the form of Vec<float,N> to
	/// values. The key must support a method 'Vec<float,N> positionOf(Key)'.
	/// The difference is that keys do not need to be equal. Instead they
	/// only need to be similar enough to be mapped to one element. The
	/// difference in each dimension should never exceed the grid-cell size
	/// in that dimension.
	///
	/// The regular performance is O(k * 3^n) where n is the number of dimensions
	/// with more than 1 cell in that dimension and k is the average number of
	/// different keys which may fit into a single cell without being equal.
	/// The space complexity is linear in the number of stored elements.
	// if diff(A,B) < x -> A==B
	// Map B -> uint
	// given (A,uint) return uint of B or add (A,uint)
	template<int N, typename Key, typename Value>
	class HashGrid
	{
	public:
		/// \param [in] _maxDist Maximum distance which will be accepted as equal.
		HashGrid(const ei::Vec<float, N>& _domainMin, const ei::Vec<float, N>& _domainMax, ei::Vec<float, N> _gridSpacing) :
			m_domainMin(_domainMin)
		{
			ei::Vec<float, N> domainSize = _domainMax - _domainMin;
			m_gridSize = floor(domainSize / _gridSpacing) + 1;
			m_domainToGrid = m_gridSize / domainSize;
		}


		/// No test if another point exists!
		void addPointFast(const Key& _key, const Value& _newValue)
		{
			uint32 hash = getHash(getGridCoord(_key));
			m_map[hash].push_back({_key, _newValue});
		}

		const Value* find(const Key& _key, const std::function<bool(const Key&,const Key&)>& _similar)
		{
			ei::Vec<int, N> gridCoord = getGridCoord(_key);
			ei::Vec<int, N> lookupCoord;
			return findRecurs<0>(_key, gridCoord, lookupCoord, _similar);
		}
	private:
		struct KVPair {Key k; Value v;};
		std::unordered_map<uint32, std::vector<KVPair>> m_map;
		ei::Vec<float, N> m_domainMin;
		ei::Vec<float, N> m_domainToGrid;
		ei::Vec<int, N> m_gridSize;

		ei::Vec<int, N> getGridCoord(const Key& _key)
		{
			ei::Vec<float, N> pos = positionOf(_key);
			return floor((pos - m_domainMin) * m_domainToGrid);
		}

		uint32 getHash(const ei::Vec<int, N>& _gridCoord)
		{
			// FNV-1a hash on the integer vector.
			// http://isthe.com/chongo/tech/comp/fnv/#FNV-1a
			uint32 hash = 2166136261;
			const char* octet = (const char*)&_gridCoord;
			for(size_t i = 0; i < sizeof(ei::Vec<int, N>); ++i)
			{
				hash ^= octet[i];
				hash *= 16777619;
			}
			return hash;
		}

		template<int N2>
		inline typename std::enable_if<N==N2, const Value*>::type
			findRecurs(const Key& _key, const ei::Vec<int, N>& _gridCoord, ei::Vec<int, N>& _lc, const std::function<bool(const Key&,const Key&)>& _similar)
		{
			uint32 hash = getHash(_lc);
			auto it = m_map.find(hash);
			if(it == m_map.end()) return nullptr;
			for(size_t i = 0; i < it->second.size(); ++i)
			{
				if(_similar(it->second[i].k, _key))
					return &it->second[i].v;
			}
			return nullptr;
		}
		template<int N2>
		inline typename std::enable_if<(N>N2), const Value*>::type
			findRecurs(const Key& _key, const ei::Vec<int, N>& _gridCoord, ei::Vec<int, N>& _lc, const std::function<bool(const Key&,const Key&)>& _similar)
		{
			for(int i = (_gridCoord[N2] > 0 ? -1 : 0); i <= (_gridCoord[N2] < m_gridSize[N2]-1 ? 1 : 0); ++i)
			{
				_lc[N2] = _gridCoord[N2] + i;
				const Value* val = findRecurs<N2+1>(_key, _gridCoord, _lc, _similar);
				if(val) return val;
			}
			return nullptr;
		}
	};

} // namespace bim