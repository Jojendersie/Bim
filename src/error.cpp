#include "error.hpp"

#include <iostream>

namespace bim {
	void defaultCallback(MessageType _type, const std::string& _message)
	{
		switch(_type)
		{
		case MessageType::INFO:    std::cerr << "INF: " << _message.c_str() << '\n'; break;
		case MessageType::WARNING: std::cerr << "WAR: " << _message.c_str() << '\n'; break;
		case MessageType::ERROR:   std::cerr << "ERR: " << _message.c_str() << '\n'; break;
		default: std::cerr << "???: " << _message.c_str() << '\n'; break;
		}
	}

	namespace details
	{
		MessageCallback g_theCallback = defaultCallback;
	}

	void setMessageCallback(MessageCallback _callback)
	{
		details::g_theCallback = _callback;
	}

}