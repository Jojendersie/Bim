#pragma once

#include <vector>

class Json
{
public:
	enum class ValueType
	{
		OBJECT,
		ARRAY,
		STRING,
		FLOAT,
		INT,
		BOOL
	};

	struct Value
	{
		ValueType getType() const { return type; }
		const char* getName() const { return name; }

		// Not type safe!
		// Be sure the type is appropriate before calling one of the following.
		const char* getString() const { return _string; }
		float getFloat() const { return _float; }
		int getInt() const { return _int; }
		bool getBool() const { return _bool; }
	private:
		friend class Json;
		ValueType type;
		const char* name;
		unsigned offset;	// Offset in the input stream
		union {
			const char* _string;
			float _float;
			int _int;
			bool _bool;
		};
	};

	// Open a file and get the root node
	bool open(const char* _file, Value& _root);
	// Go to the next node one the same level
	// [out] _next Writes output value to this buffer. Can be the same as _current.
	// Returns false if no next element can be read
	bool next(const Value& _current, Value& _next);
	// Go to the first child of the value
	// [out] _next Writes output value to this buffer. Can be the same as _current.
	// Returns false if no child element can be read
	bool child(const Value& _current, Value& _next);
private:
	char* m_readPos;
	const char* m_tokenPos; // Token has length m_readPos - m_tokenPos
	std::vector<char> m_fileContent;

	bool readProperty(Value& _next);
	bool readValue(Value& _next);
	bool readToken();
};

class JsonWriter
{
public:
	JsonWriter() : m_idention(0), m_lastMode(Mode::NEWLINE) {}
	bool open(const char* _file);
	void beginObject();
	void endObject();
	void valuePreamble(const char* _valueName);
	void value(const char* _string);
	void value(float* _floatArray, int _num);
private:
	FILE* m_outFile;
	int m_idention;

	enum class Mode {
		NEWLINE,
		PREAMBLE,
		ENDLINE
	};
	Mode m_lastMode;
};

// ************************************************************************* //
// ************************************************************************* //

bool Json::open(const char* _file, Value& _root)
{
	FILE* file = fopen(_file, "rb");
	if(!file) return false;

	// Buffer entire file
	fseek(file, 0, SEEK_END);
	long length = ftell(file);
	fseek(file, 0, SEEK_SET);
	m_fileContent.resize(length+1);
	fread(m_fileContent.data(), length, 1, file);
	m_fileContent[length] = 0;
	fclose(file);

	_root.offset = 0;
	_root.type = ValueType::OBJECT;
	_root.name = "_root_";
	return true;
}

bool Json::next(const Value& _current, Value& _next)
{
	m_readPos = m_fileContent.data() + _current.offset;
	// Iterate through the stream to the next ','. Meanwhile the number of
	// brackets ({} and []) must match.
	int parenthesis = 0;
	do {
		if(!readToken()) return false;
		if(m_tokenPos[0] == '{' || m_tokenPos[0] == '[') ++parenthesis;
		else if(m_tokenPos[0] == '}' || m_tokenPos[0] == ']') --parenthesis;
	} while(parenthesis > 0 || m_tokenPos[0] != ',');
	if(parenthesis < 0) return false;
	readProperty(_next);
	return true;
}

bool Json::child(const Value& _current, Value& _next)
{
	m_readPos = m_fileContent.data() + _current.offset;
	if(!readToken()) return false;
	if(_current.type == ValueType::ARRAY)
	{
		// Next token is '[' otherwise the type would have been wrong
		if(m_tokenPos[0] != '[') return false;
		// Read array value directly
		if(!readToken()) return false;
		return readValue(_next);
	} else if( _current.type == ValueType::OBJECT)
	{
		// Next token is '{' otherwise the type would have been wrong
		if(m_tokenPos[0] != '{') return false;
		// Subobject has a property list
		return readProperty(_next);
	}
	return false;
}

bool Json::readProperty(Value& _next)
{
	// Read "name": ...
	if(!readToken()) return false;
	if(m_tokenPos[0] != '"') return readValue(_next); // Expect to be inside an array.
	if(!readToken()) return false;
	if((m_readPos[0] != '"') && (m_readPos[0] != '\0')) return false; // syntax error
	*m_readPos = '\0'; // Force zero termination (destroys original data, but token parser can handle this).
	++m_readPos;
	_next.name = m_tokenPos;
	if(!readToken()) return false;
	if(m_tokenPos[0] != ':') return false; // syntax error
	if(!readToken()) return false;
	return readValue(_next);
}

bool Json::readValue(Value& _next)
{
	_next.offset = unsigned(m_tokenPos - m_fileContent.data());
//	if(!readToken()) return false;
	switch(m_tokenPos[0])
	{
	case 't': _next.type = ValueType::BOOL; _next._bool = true; break;
	case 'b': _next.type = ValueType::BOOL; _next._bool = false; break;
	case '"': _next.type = ValueType::STRING; if(!readToken()) return false; *m_readPos = '\0'; _next._string = m_tokenPos; break;
	case '[': _next.type = ValueType::ARRAY; break;
	case '{': _next.type = ValueType::OBJECT; break;
	default: {
		char nextChar = *m_readPos;
		*m_readPos = 0; // Zero termination for current token
		// Search the decimal '.'
		const char* c = m_tokenPos;
		while(c != m_readPos && *c != '.') ++c;
		if(*c == '.') { _next.type = ValueType::FLOAT; _next._float = (float)atof(m_tokenPos); }
		else { _next.type = ValueType::INT; _next._int = atoi(m_tokenPos); }
		*m_readPos = nextChar;
	} break;
	}
	return true;
}

bool Json::readToken()
{
	if(m_readPos == &m_fileContent.back()) return false;
	// Skip white spaces
	while(m_readPos[0] == ' ' || m_readPos[0] == '\n' || m_readPos[0] == '\r' || m_readPos[0] == '\t')
	{
		++m_readPos;
		if(m_readPos == &m_fileContent.back()) return false;
	}
	m_tokenPos = m_readPos;
	// Go as long as no separator is found
	while(m_readPos[0] != ' ' && m_readPos[0] != '\n' && m_readPos[0] != '\r' && m_readPos[0] != '\t'
		&& m_readPos[0] != ',' && m_readPos[0] != '"' && m_readPos[0] != ':'
		&& m_readPos[0] != '[' && m_readPos[0] != ']' && m_readPos[0] != '\0'
		&& m_readPos[0] != '{' && m_readPos[0] != '}')
	{
		++m_readPos;
		if(m_readPos == &m_fileContent.back()) return false;
	}
	// If the token is a printable separator its length is 0 here
	if(m_readPos == m_tokenPos) ++m_readPos;
	return true;
}

// ************************************************************************* //

bool JsonWriter::open(const char* _file)
{
	m_outFile = fopen(_file, "wb");
	return m_outFile != nullptr;
}

void JsonWriter::beginObject()
{
	if(m_lastMode == Mode::NEWLINE)
		for(int i = 0; i < m_idention; ++i)
			fwrite("\t", 1, 1, m_outFile);
	fwrite("{\n", 2, 1, m_outFile);
	++m_idention;
	m_lastMode = Mode::NEWLINE;
}

void JsonWriter::endObject()
{
	if(m_lastMode == Mode::ENDLINE)
		fwrite("\n", 1, 1, m_outFile);
	--m_idention;
	for(int i = 0; i < m_idention; ++i)
		fwrite("\t", 1, 1, m_outFile);
	fwrite("}", 1, 1, m_outFile);
	m_lastMode = Mode::ENDLINE;
}

void JsonWriter::valuePreamble(const char* _valueName)
{
	if(m_lastMode == Mode::ENDLINE)
		fwrite(",\n", 2, 1, m_outFile);
	for(int i = 0; i < m_idention; ++i)
		fwrite("\t", 1, 1, m_outFile);
	fwrite("\"", 1, 1, m_outFile);
	fwrite(_valueName, strlen(_valueName), 1, m_outFile);
	fwrite("\": ", 3, 1, m_outFile);
	m_lastMode = Mode::PREAMBLE;
}

void JsonWriter::value(const char* _string)
{
	fwrite("\"", 1, 1, m_outFile);
	fwrite(_string, strlen(_string), 1, m_outFile);
	fwrite("\"", 1, 1, m_outFile);
	m_lastMode = Mode::ENDLINE;
}

void JsonWriter::value(float* _floatArray, int _num)
{
	if(_num == 1)
		fprintf(m_outFile, "%g", _floatArray[0]);
	else {
		fprintf(m_outFile, "[");
		for(int i = 0; i < _num; ++i)
			fprintf(m_outFile, "%g%s", _floatArray[0], (i==_num-1) ? "]" : ", ");
	}
	m_lastMode = Mode::ENDLINE;
}
