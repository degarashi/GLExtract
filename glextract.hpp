#pragma once

#ifdef USE_BOOST
	#include <boost/regex.hpp>
	using boost::regex;
	using boost::smatch;
	using boost::regex_search;
	using boost::regex_replace;

	#define REP_ALNUM	"[a-zA-Z0-9_]+"
	#define REP_RET		"[a-zA-Z0-9_ \\*&]+?"
	#define REP_ARG		"[a-zA-Z0-9_][a-zA-Z0-9_\\*& ]*"
#else
	#include <regex>
	using std::regex;
	using std::smatch;
	using std::regex_search;
	using std::regex_replace;

	#define REP_ALNUM	"\\[a-zA-Z0-9_\\]+"
	#define REP_RET		"\\[a-zA-Z0-9_ \\*&\\]+?"
	#define REP_ARG		"\\[a-zA-Z0-9_\\]\\[a-zA-Z0-9_\\*& \\]*"
#endif

extern const std::string	rs_proto,
							rs_args,
							rs_define;
extern const regex			re_proto,
							re_args,
							re_define;

