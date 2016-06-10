#include <assimp/DefaultLogger.hpp>
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
	if(outputFileName.empty())
	{
		outputFileName = inputModelFile.substr(0, inputModelFile.find_last_of('.'));
	}

	// Initialize Assimp logger
	Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
	Assimp::DefaultLogger::get()->attachStream( new LogDebugStream, Assimp::Logger::Debugging);
	Assimp::DefaultLogger::get()->attachStream( new LogInfoStream, Assimp::Logger::Info);
	Assimp::DefaultLogger::get()->attachStream( new LogWarnStream, Assimp::Logger::Warn);
	Assimp::DefaultLogger::get()->attachStream( new LogErrStream, Assimp::Logger::Err);

	return 0;
}