#include "glextract.hpp"

const std::string rs_proto = "^\\s*(?:WINGDIAPI|GLAPI|GL_APICALL)\\s+(" REP_RET ")\\s*(?:APIENTRY|GL_APIENTRY|GLAPIENTRY)\\s+(" REP_ALNUM ")\\s*"	// GLAPI [1=ReturnType] APIENTRY [2=FuncName]
								"\\(((?:\\s*(?:" REP_ARG "),?)*)\\)",																				// ([3=Args...])
					rs_args = "\\s*(" REP_ARG "\\s+(?:&|\\*)?(" REP_ALNUM "))",																		// [1=ArgName]
					rs_define = "^\\s*GLDEFINE\\(\\s*(" REP_ALNUM ")";
const regex re_proto(rs_proto),
			re_args(rs_args),
			re_define(rs_define);

