#include "bim/bim.hpp"

using namespace ei;

namespace bim {

	void exportToObj(BinaryModel& _model, const char* _fileName)
	{
		std::ofstream file;
		file.open(_fileName, std::ofstream::out);
		if(file.bad()) return;

		file.setf(std::ios_base::fixed, std::ios_base::floatfield);
		file.precision(4);

		// Preamble
		file << "# Exported from bim\n\n";

		// Statistics
		//file << "# number of vertices: " << chunk->getNumVertices() << '\n';
		//file << "# number of triangles: " << indices.size() / 3 << '\n';
		//file << '\n';

		IVec3 chunkPos;
		for(chunkPos.z = 0; chunkPos.z < _model.getNumChunks().z; ++chunkPos.z)
		{
			for(chunkPos.y = 0; chunkPos.y < _model.getNumChunks().y; ++chunkPos.y)
			{
				for(chunkPos.x = 0; chunkPos.x < _model.getNumChunks().x; ++chunkPos.x)
				{
					_model.makeChunkResident(chunkPos);
					Chunk* chunk = _model.getChunk(chunkPos);

					// Vertex data
					for(uint i = 0; i < chunk->getNumVertices(); ++i)
						file << "v " << chunk->getPositions()[i].x
							 << ' ' << chunk->getPositions()[i].y
							 << ' ' << chunk->getPositions()[i].z << '\n';
					file << '\n';

					// Texture data
					if(chunk->getTexCoords0())
					for(uint i = 0; i < chunk->getNumVertices(); ++i)
						file << "vt " << chunk->getTexCoords0()[i].x
							 << ' ' << chunk->getTexCoords0()[i].y << '\n';
					file << '\n';

					// Normal data
					if(chunk->getNormals())
					for(uint i = 0; i < chunk->getNumVertices(); ++i)
						file << "vn " << chunk->getNormals()[i].x
							 << ' ' << chunk->getNormals()[i].y
							 << ' ' << chunk->getNormals()[i].z << '\n';
					file << '\n';

					// Faces
					for(size_t i = 0; i < chunk->getNumTriangles(); ++i)
					{
						auto& tr = chunk->getTriangles()[i];
						file << "f ";
						for(int j = 0; j < 3; ++j)
						{
							file << (tr[j]+1);
							if(chunk->getTexCoords0()) file << '/' << (tr[j]+1);
							if(chunk->getNormals()) file << (chunk->getTexCoords0()?"/":"//") << (tr[j]+1);
							file << (j==2?'\n':' ');
						}
					}
					file << '\n';
				}
			}
		}
		file.close();
	}

} // namespace bim