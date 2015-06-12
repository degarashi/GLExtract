#include "glextract.hpp"

namespace {
	const std::string rs_proto = "^\\s*(?:WINGDIAPI|GLAPI|GL_APICALL)\\s+(" REP_RET ")\\s*(?:APIENTRY|GL_APIENTRY|GLAPIENTRY)\\s+(" REP_ALNUM ")\\s*"	// GLAPI [1=ReturnType] APIENTRY [2=FuncName]
									"\\(((?:\\s*(?:" REP_ARG "),?)*)\\)",																				// ([3=Args...])
						rs_args = "\\s*(" REP_ARG "\\s+(?:&|\\*)?(" REP_ALNUM "))",																		// [1=ArgName]
						rs_define = "^\\s*#\\s*define.+?(" REP_ALNUM "+)\\s+0x(" REP_ALNUM ").+?$",														// [1=DefineName] [2=Value]
						rs_gldefine = "^\\s*GLDEFINE\\(\\s*(" REP_ToCamma "),\\s*" REP_ToRB "\\)",
						rs_gldefmethod = "^\\s*DEF_GLMETHOD\\(\\s*" REP_ToCamma "\\s*,\\s*" REP_ALNUM "\\s*,\\s*(" REP_ALNUM ").*?$",
						rs_gldefconst = "^\\s*DEF_GLCONST\\(\\s*(" REP_ToCamma "),\\s*" REP_ToRB "\\)";
}
const regex re_proto(rs_proto),
			re_args(rs_args),
			re_define(rs_define),
			re_gldefine(rs_gldefine),
			re_gldefmethod(rs_gldefmethod),
			re_gldefconst(rs_gldefconst);
