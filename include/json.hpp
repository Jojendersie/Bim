#pragma once

#include <vector>

class Json
{
	public:enum class ValueType
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
		~Value();
		ValueType getType() const { return type; }
		const char* getName() const { return name; }
	private:
		friend class Json;
		ValueType type;
		const char* name;
		unsigned offset;	// Offset in the input stream
		union {
			char* _string;
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

// ************************************************************************* //

Json::Value::~Value()
{
	if(type == ValueType::STRING)
		free(_string);
}

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
	if(m_tokenPos[0] != '"') return false; // syntax error
	if(!readToken()) return false;
	if(m_readPos[0] != '"') return false; // syntax error
	*m_readPos = '\0'; // Force zero termination (destroys original data, but token parser can handle this).
	++m_readPos;
	_next.name = m_tokenPos;
	if(!readToken()) return false;
	if(m_tokenPos[0] != ':') return false; // syntax error
	_next.offset = unsigned(m_readPos - m_fileContent.data()) + 1;
	readValue(_next);
	return true;
}

bool Json::readValue(Value& _next)
{
	if(!readToken()) return false;
	switch(m_tokenPos[0])
	{
	case 't': _next.type = ValueType::BOOL; _next._bool = true; break;
	case 'b': _next.type = ValueType::BOOL; _next._bool = false; break;
	case '"': _next.type = ValueType::STRING; _next._string = nullptr; break;
	case '[': _next.type = ValueType::ARRAY; break;
	case '{': _next.type = ValueType::OBJECT; break;
	default: /*INT OR FLOAT*/ break;
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