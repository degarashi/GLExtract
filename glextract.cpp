/*! glext.h から関数宣言を抜き出してファイルに書き出すプログラム
	戻り値は手抜き: 0が正常終了, 1が異常終了
	第一引数に定義ファイル(ファイルに出力させたいマクロ名を記述 (詳細は見てもらえればわかるかと))
	第二引数にglext.hを
	出力は第三引数を指定 (glfunc.inc などのファイル名を与える)
	
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
	#define DEF_GLMETHOD(ret_type, num, name, args, argnames) \
		ret_type name(BOOST_PP_SEQ_ENUM(args)) { \
			// 本当はエラーチェックなどする \
			return name(BOOST_PP_SEQ_ENUM(argnames)); \
		}
	このような感じで使うが、必要なければ空マクロにしておく
	
	オプション:
	-a	出力ファイルに追記
	-d	definition出力
	-m	method定義出力
	(デフォルトは両方)
*/

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <fstream>
const static int MAX_FILESIZE = 0x800000;
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
	return std::string();
}

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

enum ArgType {
	Arg_Define,
	Arg_Input,
	Arg_Output,
	NumArg
};
enum ExportType {
	Exp_Definition = 0x01,
	Exp_Method = 0x02,
	Exp_All = ~0
};
int g_exp = 0;
const char* g_arg[NumArg];
bool g_bAppend = false;
bool CheckArgs(int argc, char* arg[]) {
	int current = 0;
	// 引数をチェック
	for(int i=1 ; i<argc ; i++) {
		if(arg[i][0] == '-') {
			int cur = 1;
			for(;;) {
				if(arg[i][cur] == '\0')
					break;
				
				// オプション判定
				switch(arg[i][cur]) {
					// 追記モード
					case 'a':
						g_bAppend = true;
						break;
					// Definition出力
					case 'd':
						g_exp |= Exp_Definition;
						break;
					// Method出力
					case 'm':
						g_exp |= Exp_Method;
						break;
					default:
						std::cout << "error: unknown option " << arg[i] << std::endl;
						return false;
				}
				++cur;
			}
		} else
			g_arg[current++] = arg[i];
	}
	if(current < NumArg) {
		std::cout << "error: too few arguments" << std::endl;
		return false;
	}
	return true;
}
#include <unordered_set>

#ifdef USE_BOOST
	#include <boost/regex.hpp>
	using boost::regex;
	using boost::smatch;
	using boost::regex_search;
	using boost::regex_replace;

	int main(int argc, char* arg[]) {
#else
	#include <regex>
	using std::regex;
	using std::smatch;
	using std::regex_search;
	using std::regex_replace;

	int main(int argc, char* arg[]) {
#endif
	std::pair<regex, std::string> replacePair[] = {
		{regex("@C_Alnum"), "[a-zA-Z0-9_]+"},
		{regex("@C_Ret"), "[a-zA-Z0-9_ \\*&]+?"},
		{regex("@C_Arg"), "[a-zA-Z0-9_][a-zA-Z0-9_\\*& ]*"}
	};

#ifndef USE_BOOST
	{
		regex re_br("(?:\\[(.+?)\\])");
		for(auto& r : replacePair)
			r.second = regex_replace(r.second, re_br, R"(\\[$1\\])");
	}
#endif

	if(!CheckArgs(argc, arg)) {
		std::cout << "usage: glextract [-adm] [extraction definition file] [OpenGL Header(typically, glext.h)] [output filename]" << std::endl;
		return 0;
	}
	if(g_exp == 0)
		g_exp = Exp_All;

	std::string rs_proto = "^\\s*(?:WINGDIAPI|GLAPI|GL_APICALL)\\s+(@C_Ret)\\s*(?:APIENTRY|GL_APIENTRY|GLAPIENTRY)\\s+(@C_Alnum)\\s*"	// GLAPI [1=ReturnType] APIENTRY [2=FuncName]
							"\\(((?:\\s*(?:@C_Arg),?)*)\\)";																	// ([3=Args...])
	std::string rs_args = "\\s*(@C_Arg\\s+(?:&|\\*)?(@C_Alnum))";																// [1=ArgName]
	std::string rs_define = "^\\s*GLDEFINE\\(\\s*(@C_Alnum)";

	for(auto& p : replacePair) {
		rs_proto = regex_replace(rs_proto, p.first, p.second);
		rs_args = regex_replace(rs_args, p.first, p.second);
		rs_define = regex_replace(rs_define, p.first, p.second);
	}
	regex re_proto(rs_proto),
			re_args(rs_args),
			re_define(rs_define);
	try {
		// 入力ファイル内容を全部メモリにコピー
		std::ifstream ifs(g_arg[Arg_Input]);
		if(!ifs.is_open())
			throw std::runtime_error("can't open input-file");

		std::string buff = ReadAll(ifs);
		auto itr = buff.cbegin(),
			itrE = buff.cend();

		// OpenGL API定義の検出に使うRegex構文を読み込む
		std::ifstream def(g_arg[Arg_Define]);
		if(!def.is_open())
			throw std::runtime_error("can't open definition-file");
		std::ios::openmode flag = std::ios::in | std::ios::out;
		// 追記モードでなければtruncフラグを付ける
		if(!g_bAppend)
			flag |= std::ios::trunc;
		std::fstream ofs(g_arg[Arg_Output], flag);
		if(!ofs.is_open()) {
			bool bValid = false;
			// 追記モードでファイルが存在しない場合はtruncフラグ付きで新規にオープン
			if(g_bAppend) {
				ofs.open(g_arg[Arg_Output], flag | std::ios::trunc);
				bValid = ofs.is_open();
			}
			if(!bValid)
				throw std::runtime_error("can't open output-file");
		}

		smatch res;
		std::unordered_set<std::string>	funcsInFile;
		//! 出力先ファイルに含まれる関数名を登録
		if(g_bAppend) {
			std::string oBuff = ReadAll(ofs);
			auto itr = oBuff.cbegin(),
				itrE = oBuff.cend();
			while(regex_search(itr, itrE, res, re_define)) {
				funcsInFile.insert(res[1].str());
				itr = res.suffix().first;
			}
		}
		ofs.seekp(0, std::ios::end);

		std::string str[2];
		std::stringstream ss;
		int count = 0,
			skipcount = 0;
		for(;;) {
			if(def.eof())
				break;
			std::getline(def, str[0]);
			std::getline(def, str[1]);
			if(str[0].empty() || str[1].empty())
				break;
			regex re_begin(str[0]),
					re_end(str[1]);
			for(;;) {
				// 定義ファイルの記述に合った箇所まで読み飛ばし -> (読み込み始める地点)
				if(!regex_search(itr, itrE, res, re_begin))
					break;
				itr = res.suffix().first;

				// 定義の終わりを検出 -> (読み込み終わりの地点)
				if(!regex_search(itr, itrE, res, re_end))
					return 1;
				auto itrLE = res.suffix().first;
				for(;;) {
					// プロトタイプ宣言の名前を取得
					if(!regex_search(itr, itrLE, res, re_proto))
						break;
					itr = res.suffix().first;

					// 既に出力した定義名ならばスキップ
					Func func;
					func.name = res.str(2);
					func.ret_type = res.str(1);
					if(g_bAppend && funcsInFile.count(func.name)!=0) {
						++skipcount;
						continue;
					}
					++count;
					funcsInFile.insert(func.name);
					{
						auto itr = res[3].first,
							itrE = res[3].second;
						smatch res2;
						while(regex_search(itr, itrE, res2, re_args)) {
							func.arg_pair.push_back(ArgPair{res2.str(1), res2.str(2)});
							itr = res2.suffix().first;
						}
					}

					ss.str("");
					ss.clear();
					std::string funcDef;
					funcDef.resize(func.name.size());
					// タイプ定義の名前を作成
					if(g_exp & Exp_Definition) {
						// :大文字変換
						std::transform(func.name.cbegin(), func.name.cend(), funcDef.begin(), ::toupper);
						ss << "PFN" << funcDef << "PROC";
						// タイプ定義のアウトプット
						ofs << "GLDEFINE(" + func.name + ',' + ss.str() + ')' << std::endl;
					}
					// GLラッパー定義のアウトプット
					if(g_exp & Exp_Method) {
						// ret_typeがvoidなら0, それ以外は1を出力
						int bVoid = (func.ret_type == "void") ? 0 : 1;
						ofs << "DEF_GLMETHOD(" << func.ret_type << ", " << bVoid << ", " << func.name << ", ";
						if(func.arg_pair.empty())
							ofs << "(), ()";
						else {
							if(func.arg_pair[0].type_name == "void")
								ofs << "(), ()";
							else {
								for(auto& ap : func.arg_pair)
									ofs << "(" << ap.type_name << ")";
								ofs << ", ";
								for(auto& ap : func.arg_pair)
									ofs << "(" << ap.name << ")";
							}
						}
						ofs << ')' << std::endl;
					}
				}
			}
		}
		// スキップした数と出力された数を表示
		std::cout << count << " entries exported." << std::endl;
		std::cout << skipcount << " entries skipped." << std::endl;
	} catch(const std::exception& e) {
		std::cout << "error occured!" << std::endl << e.what() << std::endl;
	}
	return 0;
}
