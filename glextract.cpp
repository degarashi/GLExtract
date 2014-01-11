/*! glext.h から関数宣言を抜き出してファイルに書き出すプログラム
	戻り値は手抜き: 0が正常終了, 1が異常終了
	第一引数にglext.hを, 出力は第二引数を指定 (glfunc.inc などのファイル名を与える)
	glext.defファイルに出力させたいマクロ名を記述 (詳細は見てもらえればわかるかと)
	
	使う際は C++ヘッダに
	#define GLDEFINE(name,type)		extern type name;
	#include "glfunc.inc"
	#undef GLDEFINE

	C++ソースに
	#define GLDEFINE(name,type)		type name;
	#include "glfunc.inc"
	#undef GLDEFINE

	C++ロード関数を
	#define GLDEFINE(name,type)		name = (type)wglGetProcAddress(#name); \
		if(!name) throw std::runtime_error(std::string("error on loading GL function \"") + #name + '\"');
	void LoadWGLFunc() {
		#include "glfunc.inc"
	}
	#undef GLDEFINE
	などと宣言すると良いと思います

	GCCだとregexが未実装らしく実行時エラーを吐く（？）っぽいのでその時はboostを使うなりしてください
	（boostを使う時はUSE_BOOSTをdefineする）
	
	DEF_GLMETHOD(...)は、OpenGLの関数ラッパー用に定義
	#define DEF_GLMETHOD(ret_type, name, args, argnames) \
		ret_type name(BOOST_PP_SEQ_ENUM(args)) { \
			// 本当はエラーチェックなどする \
			return name(BOOST_PP_SEQ_ENUM(argnames)); \
		}
	このような感じで使うが、必要なければ空マクロにしておく
*/

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <fstream>
const static int MAX_FILESIZE = 0x100000;
std::string ReadAll(std::istream& st) {
	size_t sz = static_cast<size_t>(st.seekg(0, std::ios::end).tellg());
	st.seekg(0, std::ios::beg);
	if(sz > MAX_FILESIZE)
		throw std::runtime_error("too large input file");
	if(sz != 0) {
		std::string ret;
		ret.resize(sz);
		st.read(&ret[0], sz);
		return std::move(ret);
	}
	throw std::runtime_error("can't read data from stream");
}

const std::string rs_endif("^#endif");
struct ArgPair {
	std::string		type_name,
					name;
};
struct Func {
	using ArgPairL = std::vector<ArgPair>;
	ArgPairL		arg_pair;
	std::string		name,
					ret_type;
};

#ifdef USE_BOOST
	#include <boost/regex.hpp>
	const std::pair<boost::regex, std::string> replacePair[] = {
		{boost::regex("@C_Alnum"), "[a-zA-Z0-9_]+"},
		{boost::regex("@C_Arg"), "[a-zA-Z0-9_][a-zA-Z0-9_\\*& ]*"}
	};
	int main(int argc, char* arg[]) {
		using boost::regex;
		using boost::smatch;
		using boost::regex_search;
		using boost::regex_replace;
#else
	#include <regex>
	const std::pair<std::regex, std::string> replacePair[] = {
		{std::regex("@C_Alnum"), "\\[a-zA-Z0-9_\\]+"},
		{std::regex("@C_Arg"), "\\[a-zA-Z0-9_\\]\\[a-zA-Z0-9_\\*& \\]*"}
	};
	int main(int argc, char* arg[]) {
		using std::regex;
		using std::smatch;
		using std::regex_search;
		using std::regex_replace;
#endif
	if(argc != 3) {
		std::cout << "usage: glextract [OpenGL Header(typically, glext.h)] [output filename]" << std::endl;
		return 0;
	}
	std::string rs_proto = "^GLAPI\\s+(@C_Alnum)\\s+APIENTRY\\s+(@C_Alnum)\\s*"		// GLAPI [1=ReturnType] APIENTRY [2=FuncName]
							"\\(((?:\\s*(?:@C_Arg),?)*)\\)";						// ([3=Args...])
	std::string rs_args = "\\s*(@C_Arg\\s+(?:&|\\*)?(@C_Alnum))";					// [1=ArgName]
	for(auto& p : replacePair) {
		rs_proto = regex_replace(rs_proto, p.first, p.second);
		rs_args = regex_replace(rs_args, p.first, p.second);
	}
	regex re_proto(rs_proto),
			re_args(rs_args),
			re_endif(rs_endif);

	try {
		// 入力ファイル内容を全部メモリにコピー
		std::ifstream ifs(arg[1]);
		if(!ifs.is_open())
			throw std::runtime_error("can't open input-file");
		
		std::string buff = ReadAll(ifs);
		auto itr = buff.cbegin(),
			itrE = buff.cend();
			
		// ファイル名は "glext.def" で固定
		std::ifstream def("glext.def");
		std::ofstream ofs(arg[2]);
		if(!ofs.is_open())
			throw std::runtime_error("can't open output-file");
		
		std::string str;
		std::stringstream ss;
		smatch res;
		for(;;) {
			std::getline(def, str);
			if(str.empty())
				break;
			
			ss.str("");
			ss.clear();
			
			ss << "^#define\\s+" << str << "\\s+1";
			regex re_version(ss.str());
			for(;;) {
				// GLx_yのヘッダまで読み飛ばし
				if(!regex_search(itr, itrE, res, re_version))
					break;
				itr = res.suffix().first;

				// 定義の終わりを検出
				if(!regex_search(itr, itrE, res, re_endif))
					return 1;
				auto itrLE = res.suffix().first;
				for(;;) {
					// プロトタイプ宣言の名前を取得
					if(!regex_search(itr, itrLE, res, re_proto))
						break;
					itr = res.suffix().first;

					Func func;
					func.name = res.str(2);
					func.ret_type = res.str(1);
					{
						auto itr = res[3].first,
							itrE = res[3].second;
						smatch res2;
						while(regex_search(itr, itrLE, res2, re_args)) {
							func.arg_pair.push_back(ArgPair{res2.str(1), res2.str(2)});
							itr = res2.suffix().first;
						}
					}

					ss.str("");
					std::string funcDef;
					funcDef.resize(func.name.size());
					// タイプ定義の名前を作成
					// :大文字変換
					std::transform(func.name.cbegin(), func.name.cend(), funcDef.begin(), ::toupper);
					ss << "PFN" << funcDef << "PROC";
					// タイプ定義のアウトプット
					ofs << "GLDEFINE(" + func.name + ',' + ss.str() + ')' << std::endl;

					// GLラッパー定義のアウトプット
					ofs << "DEF_GLMETHOD(" << func.ret_type << ", " << func.name << ", ";
					for(auto& ap : func.arg_pair)
						ofs << "(" << ap.type_name << ")";
					ofs << ", ";
					for(auto& ap : func.arg_pair)
						ofs << "(" << ap.name << ")";
					ofs << ')' << std::endl;
				}
			}
		}
	} catch(const std::exception& e) {
		std::cout << "error occured!" << std::endl << e.what() << std::endl;
	}
	return 0;
}
