#include <assimp/DefaultLogger.hpp>
#include <assimp/matrix4x4.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <assimp/Importer.hpp>
#include <string>
//#include <algorithm>
#include <chrono>

#include "bim/bim.hpp"
#include "bim/log.hpp"

using namespace std::chrono;

// Assimp logging interfacing
class LogDebugStream: public Assimp::LogStream {
	void write(const char* message) override { bim::sendMessage(bim::MessageType::INFO, "[ASSIMP DBG] ", message); }
};
class LogInfoStream: public Assimp::LogStream {
	void write(const char* message) override { bim::sendMessage(bim::MessageType::INFO, "[ASSIMP] ", message); }
};
class LogWarnStream: public Assimp::LogStream {
	void write(const char* message) override { bim::sendMessage(bim::MessageType::WARNING, "[ASSIMP] ", message); }
};
class LogErrStream: public Assimp::LogStream {
	void write(const char* message) override { bim::sendMessage(bim::MessageType::ERROR, "[ASSIMP] ", message); }
};


bool loadScene(const std::string& _fileName, Assimp::Importer& _importer, bool _flipUV)
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
		//aiProcess_CalcTangentSpace		|
		aiProcess_Triangulate			|
		//aiProcess_GenSmoothNormals		|	// TODO: is angle 80% already set?
		aiProcess_ValidateDataStructure	|
		aiProcess_SortByPType			|
		//aiProcess_RemoveRedundantMaterials	|
		//aiProcess_FixInfacingNormals	|
		aiProcess_FindInvalidData		|
		aiProcess_GenUVCoords			|
		aiProcess_TransformUVCoords		|
//		aiProcess_ImproveCacheLocality	|
//		aiProcess_JoinIdenticalVertices	|
		(_flipUV ? aiProcess_FlipUVs : 0) );

	return _importer.GetScene() != nullptr;
}

void importGeometry(const aiScene* _scene, const aiNode* _node, const ei::Mat4x4& _transformation, bim::BinaryModel& _bim)
{
	_bim.makeChunkResident(ei::IVec3(0));
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
		int mi = _bim.getUniqueMaterialIndex(aiName.C_Str());
		if(mi >= 0) materialIndex = mi;
		else bim::sendMessage(bim::MessageType::WARNING, "Could not find the mesh material ", aiName.C_Str(), "!");

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
				if(mesh->HasNormals())
				{
					memcpy(&tmp, &mesh->mNormals[face.mIndices[j]], sizeof(ei::Vec3));
					newVertex[j].normal = ei::transform(tmp, invTransTransform);
				} else newVertex[j].normal = ei::Vec3(0.0f);
				if(mesh->HasTangentsAndBitangents())
				{
					memcpy(&tmp, &mesh->mTangents[face.mIndices[j]], sizeof(ei::Vec3));
					newVertex[j].tangent = ei::transform(tmp, invTransTransform);
					memcpy(&tmp, &mesh->mBitangents[face.mIndices[j]], sizeof(ei::Vec3));
					newVertex[j].bitangent = ei::transform(tmp, invTransTransform);
				} else newVertex[j].tangent = newVertex[j].bitangent = ei::Vec3(0.0f);
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
			bim::sendMessage(bim::MessageType::WARNING, "Skipped ", skippedTriangles, " degenerated triangles in mesh ", _node->mMeshes[i]);
	}

	// Import all child nodes
	for(uint i = 0; i < _node->mNumChildren; ++i)
		importGeometry(_scene, _node->mChildren[i], nodeTransform, _bim);
}

void importMaterials(const struct aiScene* _scene, bim::BinaryModel& _bim)
{
	if( !_scene->HasMaterials() )
	{
		bim::sendMessage(bim::MessageType::ERROR, "The scene has no materials to import!");
		return;
	}

	for(uint i = 0; i < _scene->mNumMaterials; ++i)
	{
		auto mat = _scene->mMaterials[i];
		aiString aiTmpStr;
		// Get name and create new material with that name
		mat->Get( AI_MATKEY_NAME, aiTmpStr );
		//bim::Material material(aiTmpStr.C_Str(), "legacy");

		// Check if the material was imported before
		bim::Material* material = _bim.getMaterial(aiTmpStr.C_Str());
		if(!material)
		{
			material = _bim.addMaterial(bim::Material(aiTmpStr.C_Str(), "legacy"));
		}
		if(material->getType() != "legacy")
			continue;

		// Load diffuse
		if(!material->has("albedo")) // Check: if already existent do not change
		{
			if( mat->GetTexture( aiTextureType_DIFFUSE, 0, &aiTmpStr ) == AI_SUCCESS )
			{
				material->setTexture("albedo", aiTmpStr.C_Str());
			} else {
				aiColor3D color = aiColor3D( 0.0f, 0.0f, 0.0f );
				mat->Get( AI_MATKEY_COLOR_DIFFUSE, color );
				material->set("albedo", ei::Vec3(color.r, color.g, color.b));
			}
		}

		// Load specular
		if(!material->has("specularColor"))
		{
			if( mat->GetTexture( aiTextureType_SPECULAR, 0, &aiTmpStr ) == AI_SUCCESS )
			{
				material->setTexture("specularColor", aiTmpStr.C_Str());
			} else {
				aiColor3D color = aiColor3D( 0.0f, 0.0f, 0.0f );
				if( mat->Get( AI_MATKEY_COLOR_REFLECTIVE, color ) != AI_SUCCESS )
					mat->Get( AI_MATKEY_COLOR_SPECULAR, color );
				material->set("specularColor", ei::Vec3(color.r, color.g, color.b));
			}
		}
		if(!material->has("roughness"))
		{
			if( mat->GetTexture( aiTextureType_SHININESS, 0, &aiTmpStr ) == AI_SUCCESS )
			{
				//material.setTexture("shininess", aiTmpStr.C_Str());
				material->set("roughness", 1.0f);
			} else {
				float shininess = 1.0f;
				mat->Get( AI_MATKEY_SHININESS, shininess );
				//material.set("reflectivity", shininess);
				material->set("roughness", 1.0f / (shininess * shininess));
			}
		}

		// Load opacity
		if(!material->has("opacity")) 
		{
			if( mat->GetTexture( aiTextureType_OPACITY, 0, &aiTmpStr ) == AI_SUCCESS )
			{
				material->setTexture("opacity", aiTmpStr.C_Str());
			} else {
				float opacity = 1.0f;
				mat->Get( AI_MATKEY_OPACITY, opacity );
				material->set("opacity", opacity);
			}
		}

		// Load emissivity
		if(!material->has("emissivity"))
		{
			if( mat->GetTexture( aiTextureType_EMISSIVE, 0, &aiTmpStr ) == AI_SUCCESS )
			{
				material->setTexture("emissivity", aiTmpStr.C_Str());
			} else {
				aiColor3D color = aiColor3D( 0.0f, 0.0f, 0.0f );
				mat->Get( AI_MATKEY_COLOR_EMISSIVE, color );
				material->set("emissivity", ei::Vec3(color.r, color.g, color.b));
			}
		}
	}
}

int main(int _numArgs, const char** _args)
{
	// Analyze all input arguments and store results in some variables
	std::string inputModelFile;
	std::string outputFileName;
	bim::Chunk::BuildMethod method = bim::Chunk::BuildMethod::SBVH;
	ei::IVec3 chunkGridRes(1);
	bool computeAAB = false;
	bool computeOB = false;
	bool computeSGGX = false;
	bool flipUV = false;
	uint maxNumTrianglesPerLeaf = 2;
	// Parse arguments now
	for(int i = 1; i < _numArgs; ++i)
	{
		if(_args[i][0] != '-') { bim::sendMessage(bim::MessageType::WARNING, "Ignoring input ", _args[i]); continue; }
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
		case 'f': if(strcmp("lipUV", _args[i] + 2) == 0) flipUV = true;
			break;
		case 't': maxNumTrianglesPerLeaf = atoi(_args[i] + 2);
			break;
		default:
			bim::sendMessage(bim::MessageType::WARNING, "Unknown option in argument ", _args[i]);
		}
	}

	// Consistency check of input arguments
	if(inputModelFile.empty()) { bim::sendMessage(bim::MessageType::ERROR, "Input file must be given!"); return 1; }
	if(!(computeAAB || computeOB)) { bim::sendMessage(bim::MessageType::ERROR, "No BVH type is given!"); return 1; }
	if(chunkGridRes < 1) { bim::sendMessage(bim::MessageType::ERROR, "Invalid grid resolution!"); return 1; }

	// Derive output file name
	std::string outputBimFile, outputJsonFile;
	if(outputFileName.empty())
	{
		outputFileName = inputModelFile.substr(0, inputModelFile.find_last_of('.'));
		outputBimFile = outputFileName + ".bim";
		outputJsonFile = outputFileName + ".json";
	}

	auto t0 = high_resolution_clock::now();

	// Import Assimp scene
	Assimp::Importer importer;
	if(!loadScene(inputModelFile, importer, flipUV))
	{
		bim::sendMessage(bim::MessageType::ERROR, "Failed to import the scene with assimp.");
		return 1;
	}

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
	auto t1 = high_resolution_clock::now();
	bim::sendMessage(bim::MessageType::INFO, "Finished Assimp loading in ", duration_cast<duration<float>>(t1-t0).count() , " s\n",
		"    Meshes: ", importer.GetScene()->mNumMeshes, "\n",
		"    Materials: ", importer.GetScene()->mNumMaterials, "\n",
		"    Vertices: ", numVertices, "\n",
		"    Triangles: ", numTriangles);
	bim::BinaryModel model(properties, chunkGridRes);
	model.loadEnvironmentFile(outputJsonFile.c_str());
	// Fill the model with data
	bim::sendMessage(bim::MessageType::INFO, "importing materials...");
	importMaterials(importer.GetScene(), model);
	bim::sendMessage(bim::MessageType::INFO, "importing geometry...");
	importGeometry(importer.GetScene(), importer.GetScene()->mRootNode, ei::identity4x4(), model);
	importer.FreeScene();
	auto t2 = high_resolution_clock::now();
	bim::sendMessage(bim::MessageType::INFO, "Finished importing geometry to bim in ", duration_cast<duration<float>>(t2-t1).count(), " s");
	// Compute additional data
	bim::sendMessage(bim::MessageType::INFO, "recomputing bounding box...");
	model.refreshBoundingBox();
	auto t3 = high_resolution_clock::now();
	bim::sendMessage(bim::MessageType::INFO, "Finished bounding box in ", duration_cast<duration<float>>(t3-t2).count(), " s");
	//foreach chunk
	{
		bim::sendMessage(bim::MessageType::INFO, "removing redundant vertices...");
		model.getChunk(ei::IVec3(0))->removeRedundantVertices();
		bim::sendMessage(bim::MessageType::INFO, "computing tangent space...");
		model.getChunk(ei::IVec3(0))->computeTangentSpace(bim::Property::Val(bim::Property::NORMAL | bim::Property::TANGENT | bim::Property::BITANGENT), true);
		bim::sendMessage(bim::MessageType::INFO, "building BVH...");
		t0 = high_resolution_clock::now();
		model.getChunk(ei::IVec3(0))->buildHierarchy(method, maxNumTrianglesPerLeaf);
		t1 = high_resolution_clock::now();
		bim::sendMessage(bim::MessageType::INFO, "Finished BVH structure in ", duration_cast<duration<float>>(t1-t0).count(), " s\n",
				"    Max. tree depth: ", model.getChunk(ei::IVec3(0))->getNumTreeLevels());

		if(computeAAB && method != bim::Chunk::BuildMethod::SBVH) {
			bim::sendMessage(bim::MessageType::INFO, "computing AABoxes...");
			model.getChunk(ei::IVec3(0))->computeBVHAABoxes();
		}
		if(computeOB) {
			bim::sendMessage(bim::MessageType::INFO, "computing OBoxes...");
			model.getChunk(ei::IVec3(0))->computeBVHOBoxes();
		}
		if(computeSGGX) {
			bim::sendMessage(bim::MessageType::INFO, "computing SGGX NDFs...");
			model.getChunk(ei::IVec3(0))->computeBVHSGGXApproximations();
		}

		t2 = high_resolution_clock::now();
		bim::sendMessage(bim::MessageType::INFO, "Finished BVH nodes in ", duration_cast<duration<float>>(t2-t1).count(), " s");
	}
	// Set an accelerator if possible. Prefer AABOX (last line will win if multiple BVH are given)
	if(computeOB) model.setAccelerator(bim::Property::OBOX_BVH);
	if(computeAAB) model.setAccelerator(bim::Property::AABOX_BVH);

	// Add some default paramaters
	if(model.getNumScenarios() == 0)
	{
		auto scenario = model.addScenario("default");
		auto light = std::make_shared<bim::PointLight>(ei::Vec3(0.5f, 1.5f, 0.0f), ei::Vec3(2.0f), "defaultPL");
		auto cam = std::make_shared<bim::PerspectiveCamera>(ei::Vec3(0.0f, 0.5f, -1.0f), ei::Vec3(0.0f), ei::Vec3(0.0f, 1.0f, 0.0f), 0.5f, "defaultCam");
		model.addLight(light);
		model.addCamera(cam);
		scenario->addLight(light);
		scenario->setCamera(cam);
	}

	bim::sendMessage(bim::MessageType::INFO, "storing model...");
	model.storeEnvironmentFile(outputJsonFile.c_str(), outputBimFile.c_str());
	model.storeBinaryHeader(outputBimFile.c_str());
	//foreach chunk
	model.storeChunk(outputBimFile.c_str(), ei::IVec3(0));
	return 0;
}