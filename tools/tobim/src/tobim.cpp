#include <assimp/DefaultLogger.hpp>
#include <iostream>

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
	// Initialize Assimp logger
	Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
	Assimp::DefaultLogger::get()->attachStream( new LogDebugStream, Assimp::Logger::Debugging);
	Assimp::DefaultLogger::get()->attachStream( new LogInfoStream, Assimp::Logger::Info);
	Assimp::DefaultLogger::get()->attachStream( new LogWarnStream, Assimp::Logger::Warn);
	Assimp::DefaultLogger::get()->attachStream( new LogErrStream, Assimp::Logger::Err);

	return 0;
}