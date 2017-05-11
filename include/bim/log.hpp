#pragma once

#include <string>

namespace bim
{
	enum class MessageType
	{
		INFO,
		WARNING,
		ERROR
	};
	
	typedef void (*MessageCallback)(MessageType _type, const std::string& _message);

	// Replace the internal message callback with a custom one.
	// The default callback will output to std::cerr.
	void setMessageCallback(MessageCallback _callback);

	namespace details
	{
		extern MessageCallback g_theCallback;

		inline void buildMessageString(std::string&) {}
		template<typename T, typename... TArgs>
		void buildMessageString(std::string& _message, T _first, TArgs... _args)
		{
			_message += std::to_string(_first);
			buildMessageString(_message, _args...);
		}
		template<typename... TArgs>
		void buildMessageString(std::string& _message, const char* _first, TArgs... _args)
		{
			_message += _first;
			buildMessageString(_message, _args...);
		}
		template<typename... TArgs>
		void buildMessageString(std::string& _message, const std::string& _first, TArgs... _args)
		{
			_message += _first;
			buildMessageString(_message, _args...);
		}
	}

	template<typename... TArgs>
	void sendMessage(MessageType _type, TArgs... _args)
	{
		std::string msg;
		details::buildMessageString(msg, _args...);
		details::g_theCallback(_type, msg);
	}
}