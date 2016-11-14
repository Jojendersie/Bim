#include <assimp/DefaultLogger.hpp>
#include <assimp/matrix4x4.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <assimp/Importer.hpp>
#include <iostream>
#include <string>
//#include <algorithm>

#include "bim.hpp"

// Assimp logging interfacing
class LogDebugStream: public Assimp::LogStream {
	void write(const char* message) override { std::cerr << "ASSIMP DBG: " << message; }
};
class LogInfoStream: public Assimp::LogStream {
	void write(const char* message) override { std::cerr << "ASSIMP INF: " << message; }
};
class LogWarnStream: public Assimp::LogStream {
	void write(const char* message) override { std::cerr << "ASSIMP WAR: " << message; }
};
class LogErrStream: public Assimp::LogStream {
	void write(const char* message) override { std::cerr << "ASSIMP ERR: " << message; }
};


bool loadScene(const std::string& _fileName, Assimp::Importer& _importer)
{
	// Initialize Assimp logger
	Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
//	Assimp::DefaultLogger::get()->attachStream( new LogDebugStream, Assimp::Logger::Debugging);
	Assimp::DefaultLogger::get()->attachStream( new LogInfoStream, Assimp::Logger::Info);
	Assimp::DefaultLogger::get()->attachStream( new LogWarnStream, Assimp::Logger::Warn);
	Assimp::DefaultLogger::get()->attachStream( new LogErrStream, Assimp::Logger::Err);

	// Ignore line/point primitives
	_importer.SetPropertyInteger( AI_CONFIG_PP_SBP_REMOVE,
		aiPrimitiveType_POINT | aiPrimitiveType_LINE );

	_importer.ReadFile( _fileName,
		aiProcess_GenSmoothNormals		|
		aiProcess_CalcTangentSpace		|
		aiProcess_Triangulate			|
		//aiProcess_GenSmoothNormals		|	// TODO: is angle 80% already set?
		aiProcess_ValidateDataStructure	|
		aiProcess_SortByPType			|
		//aiProcess_RemoveRedundantMaterials	|
		//aiProcess_FixInfacingNormals	|
		aiProcess_FindInvalidData		|
		aiProcess_GenUVCoords			|
		aiProcess_TransformUVCoords		|
		aiProcess_ImproveCacheLocality	|
		aiProcess_JoinIdenticalVertices	|
		aiProcess_FlipUVs );

	return _importer.GetScene() != nullptr;
}

void importGeometry(const aiScene* _scene, const aiNode* _node, const ei::Mat4x4& _transformation, bim::BinaryModel& _bim)
{
	bim::Chunk& chunk = *_bim.getChunk(ei::IVec3(0));
	// Compute scene graph transformation
	ei::Mat4x4 nodeTransform; memcpy(&nodeTransform, &_node->mTransformation, sizeof(ei::Mat4x4));
	nodeTransform *= _transformation;
	ei::Mat3x3 invTransTransform(nodeTransform);
	invTransTransform = transpose(invert(invTransTransform));

	// Import meshes
	for(uint i = 0; i < _node->mNumMeshes; ++i)
	{
		const aiMesh* mesh = _scene->mMeshes[ _node->mMeshes[i] ];
		bim::Chunk::FullVertex newVertex[3];
		uint32 skippedTriangles = 0;

		// Find the material entry
		uint32 materialIndex = 0;
		aiString aiName;
		_scene->mMaterials[mesh->mMaterialIndex]->Get( AI_MATKEY_NAME, aiName );
		int mi = _bim.findMaterial(aiName.C_Str());
		if(mi > 0) materialIndex = mi;
		else std::cerr << "WAR: Could not find the mesh material " << aiName.C_Str() << "!\n";

		// Import and transform the geometry
		for(uint t = 0; t < mesh->mNumFaces; ++t)
		{
			const aiFace& face = mesh->mFaces[t];
			eiAssert( face.mNumIndices == 3, "This is a triangle importer!" );
			ei::UVec3 triangleIndices;
			for(int j = 0; j < 3; ++j)
			{
				// Fill the vertex
				ei::Vec3 tmp;
				memcpy(&tmp, &mesh->mVertices[face.mIndices[j]], sizeof(ei::Vec3));
				newVertex[j].position = ei::transform(tmp, nodeTransform);
				memcpy(&tmp, &mesh->mNormals[face.mIndices[j]], sizeof(ei::Vec3));
				newVertex[j].normal = ei::transform(tmp, invTransTransform);
				if(mesh->HasTangentsAndBitangents())
				{
					memcpy(&tmp, &mesh->mTangents[face.mIndices[j]], sizeof(ei::Vec3));
					newVertex[j].tangent = ei::transform(tmp, invTransTransform);
					memcpy(&tmp, &mesh->mBitangents[face.mIndices[j]], sizeof(ei::Vec3));
					newVertex[j].bitangent = ei::transform(tmp, invTransTransform);
				}
				if(mesh->HasTextureCoords(0)) memcpy(&newVertex[j].texCoord0, &mesh->mTextureCoords[0][face.mIndices[j]], sizeof(ei::Vec2));
				if(mesh->HasTextureCoords(1)) memcpy(&newVertex[j].texCoord1, &mesh->mTextureCoords[1][face.mIndices[j]], sizeof(ei::Vec2));
				if(mesh->HasTextureCoords(2)) memcpy(&newVertex[j].texCoord2, &mesh->mTextureCoords[2][face.mIndices[j]], sizeof(ei::Vec2));
				if(mesh->HasTextureCoords(3)) memcpy(&newVertex[j].texCoord3, &mesh->mTextureCoords[3][face.mIndices[j]], sizeof(ei::Vec2));
				if(mesh->HasVertexColors(0)) memcpy(&newVertex[j].color, &mesh->mColors[0][face.mIndices[j]], sizeof(ei::Vec2));
			}
			// Detect degenerated triangles
			float area2 = len(cross(newVertex[1].position - newVertex[0].position, newVertex[2].position - newVertex[0].position));
			if(abs(area2) > 1e-10f)
			{
				for(int j = 0; j < 3; ++j)
				{
					// Add vertex and get its index for the triangle
					triangleIndices[j] = chunk.getNumVertices();
					chunk.addVertex(newVertex[j]);
				}
				chunk.addTriangle(triangleIndices, materialIndex);
			} else
				skippedTriangles++;
		}

		if(skippedTriangles)
			std::cerr << "WAR: Skipped " << skippedTriangles << " degenerated triangles in mesh " << _node->mMeshes[i]  << "\n";
	}

	// Import all child nodes
	for(uint i = 0; i < _node->mNumChildren; ++i)
		importGeometry(_scene, _node->mChildren[i], nodeTransform, _bim);
}

void importMaterials(const struct aiScene* _scene, bim::BinaryModel& _bim)
{
	if( !_scene->HasMaterials() )
	{
		std::cerr << "ERR: The scene has no materials to import!\n";
		return;
	}

	for(uint i = 0; i < _scene->mNumMaterials; ++i)
	{
		auto mat = _scene->mMaterials[i];
		aiString aiTmpStr;
		// Get name and create new material with that name
		mat->Get( AI_MATKEY_NAME, aiTmpStr );
		bim::Material material(aiTmpStr.C_Str(), "legacy");

		// Check if the material was imported before
		if(_bim.findMaterial(material.getName()) != -1)
			continue;

		// Load diffuse
		if( mat->GetTexture( aiTextureType_DIFFUSE, 0, &aiTmpStr ) == AI_SUCCESS )
		{
			material.setTexture("albedo", aiTmpStr.C_Str());
		} else {
			aiColor3D color = aiColor3D( 0.0f, 0.0f, 0.0f );
			mat->Get( AI_MATKEY_COLOR_DIFFUSE, color );
			material.set("albedo", ei::Vec3(color.r, color.g, color.b));
		}

		// Load specular
		// TODO: roughness?
		if( mat->GetTexture( aiTextureType_SPECULAR, 0, &aiTmpStr ) == AI_SUCCESS )
		{
			material.setTexture("specularColor", aiTmpStr.C_Str());
		} else {
			aiColor3D color = aiColor3D( 0.0f, 0.0f, 0.0f );
			if( mat->Get( AI_MATKEY_COLOR_REFLECTIVE, color ) != AI_SUCCESS )
				mat->Get( AI_MATKEY_COLOR_SPECULAR, color );
			material.set("specularColor", ei::Vec3(color.r, color.g, color.b));
		}
		if( mat->GetTexture( aiTextureType_SHININESS, 0, &aiTmpStr ) == AI_SUCCESS )
		{
			material.setTexture("shininess", aiTmpStr.C_Str());
			material.set("roughness", 1.0f);
		} else {
			float shininess = 1.0f;
			mat->Get( AI_MATKEY_SHININESS, shininess );
			material.set("reflectivity", shininess);
			material.set("roughness", 1.0f / (shininess * shininess));
		}

		// Load opacity
		if( mat->GetTexture( aiTextureType_OPACITY, 0, &aiTmpStr ) == AI_SUCCESS )
		{
			material.setTexture("opacity", aiTmpStr.C_Str());
		} else {
			float opacity = 1.0f;
			mat->Get( AI_MATKEY_OPACITY, opacity );
			material.set("opacity", opacity);
		}

		// Load emissivity
		if( mat->GetTexture( aiTextureType_EMISSIVE, 0, &aiTmpStr ) == AI_SUCCESS )
		{
			material.setTexture("emissivity", aiTmpStr.C_Str());
		} else {
			aiColor3D color = aiColor3D( 0.0f, 0.0f, 0.0f );
			mat->Get( AI_MATKEY_COLOR_EMISSIVE, color );
			material.set("emissivity", ei::Vec3(color.r, color.g, color.b));
		}

		_bim.addMaterial(material);
	}
}

int main(int _numArgs, const char** _args)
{
	// Analyze all input arguments and store results in some variables
	std::string inputModelFile;
	std::string outputFileName;
	bim::Chunk::BuildMethod method = bim::Chunk::BuildMethod::SAH;
	ei::IVec3 chunkGridRes(1);
	bool computeAAB = false;
	bool computeOB = false;
	bool computeSGGX = false;
	// Parse arguments now
	for(int i = 1; i < _numArgs; ++i)
	{
		if(_args[i][0] != '-') { std::cerr << "WAR: Ignoring input " << _args[i] << '\n'; continue; }
		switch(_args[i][1])
		{
		case 'i': inputModelFile = _args[i] + 2;
			break;
		case 'o': outputFileName = _args[i] + 2;
			break;
		case 'b':
			if(strcmp("AAB", _args[i] + 2) == 0) computeAAB = true;
			if(strcmp("OB", _args[i] + 2) == 0) computeOB = true;
			break;
		case 'c': if(strcmp("SGGX", _args[i] + 2) == 0) computeSGGX = true;
			break;
		default:
			std::cerr << "WAR: Unknown option in argument " << _args[i] << '\n';
		}
	}

	// Consistency check of input arguments
	if(inputModelFile.empty()) { std::cerr << "ERR: Input file must be given!\n"; return 1; }
	if(!(computeAAB || computeOB)) { std::cerr << "ERR: No BVH type is given!\n"; return 1; }
	if(any(chunkGridRes < 1)) { std::cerr << "ERR: Invalid grid resolution!\n"; return 1; }

	// Derive output file name
	std::string outputBimFile, outputJsonFile;
	if(outputFileName.empty())
	{
		outputFileName = inputModelFile.substr(0, inputModelFile.find_last_of('.'));
		outputBimFile = outputFileName + ".bim";
		outputJsonFile = outputFileName + ".json";
	}

	// Import Assimp scene
	Assimp::Importer importer;
	if(!loadScene(inputModelFile, importer)) { std::cerr << "ERR: Failed to import the scene with assimp."; return 1; }
	std::cerr << "INF: Finished Assimp loading\n";
	std::cerr << "    Meshes: " << importer.GetScene()->mNumMeshes << '\n';
	std::cerr << "    Materials: " << importer.GetScene()->mNumMaterials << '\n';

	// Analyze input data to create a proper model
	bim::Property::Val properties = bim::Property::Val(bim::Property::POSITION | bim::Property::TRIANGLE_IDX | bim::Property::TRIANGLE_MAT);
	uint numVertices = 0;
	uint numTriangles = 0;
	for(uint i = 0; i < importer.GetScene()->mNumMeshes; ++i)
	{
		auto mesh = importer.GetScene()->mMeshes[i];
		if( mesh->GetNumUVChannels() > 0 ) properties = bim::Property::Val(properties | bim::Property::TEXCOORD0);
		if( mesh->GetNumUVChannels() > 1 ) properties = bim::Property::Val(properties | bim::Property::TEXCOORD1);
		if( mesh->GetNumUVChannels() > 2 ) properties = bim::Property::Val(properties | bim::Property::TEXCOORD2);
		if( mesh->GetNumUVChannels() > 3 ) properties = bim::Property::Val(properties | bim::Property::TEXCOORD3);
		if( mesh->GetNumColorChannels() > 0 ) properties = bim::Property::Val(properties | bim::Property::COLOR);
		if( mesh->HasNormals() ) properties = bim::Property::Val(properties | bim::Property::NORMAL);
		if( mesh->HasTangentsAndBitangents() ) properties = bim::Property::Val(properties | bim::Property::TANGENT | bim::Property::BITANGENT );
		// TODO: Qormals

		numVertices += mesh->mNumVertices;
		numTriangles += mesh->mNumFaces;
	}
	std::cerr << "    Vertices: " << numVertices << '\n';
	std::cerr << "    Triangles: " << numTriangles << '\n';
	bim::BinaryModel model(properties);
	// TODO: argument
	model.setNumTrianglesPerLeaf(2);
	// Fill the model with data
	std::cerr << "INF: importing materials...\n";
	importMaterials(importer.GetScene(), model);
	std::cerr << "INF: importing geometry...\n";
	importGeometry(importer.GetScene(), importer.GetScene()->mRootNode, ei::identity4x4(), model);
	importer.FreeScene();
	// Compute additional data
	std::cerr << "INF: recomputing bounding box...\n";
	model.refreshBoundingBox();
	if(prod(chunkGridRes) > 1)
	{
		std::cerr << "INF: dividing into chunks...\n";
		model.split(chunkGridRes);
	}
	//foreach chunk
	{
		std::cerr << "INF: removing redundant vertices...\n";
		model.getChunk(ei::IVec3(0))->removeRedundantVertices();
		std::cerr << "INF: computing tangent space...\n";
		model.getChunk(ei::IVec3(0))->computeTangentSpace(bim::Property::Val(bim::Property::NORMAL | bim::Property::TANGENT | bim::Property::BITANGENT));
		std::cerr << "INF: building BVH...\n";
		model.getChunk(ei::IVec3(0))->buildHierarchy(method);
		if(computeAAB) {
			std::cerr << "INF: computing AABoxes...\n";
			model.getChunk(ei::IVec3(0))->computeBVHAABoxes();
		}
		if(computeOB) {
			std::cerr << "INF: computing OBoxes...\n";
			model.getChunk(ei::IVec3(0))->computeBVHOBoxes();
		}
		if(computeSGGX) {
			std::cerr << "INF: computing SGGX NDFs...\n";
			model.getChunk(ei::IVec3(0))->computeBVHSGGXApproximations();
		}
	}
	// Set an accelerator if possible. Prefer AABOX (last line will win if multiple BVH are given)
	if(computeOB) model.setAccelerator(bim::Property::OBOX_BVH);
	if(computeAAB) model.setAccelerator(bim::Property::AABOX_BVH);

	std::cerr << "INF: storing model...\n";
	model.storeEnvironmentFile(outputJsonFile.c_str(), outputBimFile.c_str());
	model.storeBinaryHeader(outputBimFile.c_str());
	//foreach chunk
	model.storeChunk(outputBimFile.c_str(), ei::IVec3(0));
	return 0;
}