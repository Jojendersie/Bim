#pragma once
#include <map>
#include <string>

// just function declarations: myEnumToString + myEnumFromString
#define ENUM_CONVERT_FUNC(myEnum) \
static std::string myEnum##ToString( myEnum s); \
static myEnum myEnum##FromString(const std::string& s); \
static std::map< myEnum, std::string > s_##myEnum##Map;

/* 
	function definitions of myEnumToString + myEnumFromString
	-- put this in a cpp because it creates a helper map called s_myEnum
		with {myEnum, string} pairs

	notFound: Element that should be returned if no string matches e.g. myEnum::Size
	...: Pairs: {Color::Red,"red"},{Color::Blue,"blue"}
*/
#define ENUM_CONVERT(scope, myEnum, notFound,...)	\
std::map< scope::myEnum, std::string > scope::s_##myEnum##Map = {	\
		__VA_ARGS__	};						\
std::string scope::myEnum##ToString( myEnum s){	\
	auto it = s_##myEnum##Map.find(s);			\
	if(it != s_##myEnum##Map.end()) return it->second;	\
	return "";}								\
scope::myEnum scope::myEnum##FromString(const std::string& s){	\
	for(const auto& e : s_##myEnum##Map){		\
	if(e.second == s) return e.first;}		\
	return scope::notFound;}