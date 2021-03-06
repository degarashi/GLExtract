﻿/*! glext.h から関数宣言を抜き出してファイルに書き出すプログラム
	戻り値は手抜き: 0が正常終了, 1が異常終了
	第一引数に定義ファイル(ファイルに出力させたいマクロ名を記述 (詳細は見てもらえればわかるかと))
	第二引数にglext.hを
	出力は第三引数を指定 (glfunc.inc などのファイル名を与える)

	---- Definition出力 ----
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

	---- Method定義出力 ----
	DEF_GLMETHOD(...)は、OpenGLの関数ラッパー用に定義
	#define DEF_GLMETHOD(ret_type, num, name, args, argnames) \
		ret_type name(BOOST_PP_SEQ_ENUM(args)) { \
			// 本当はエラーチェックなどする \
			return name(BOOST_PP_SEQ_ENUM(argnames)); \
		}
	このような感じで使うが、必要なければ空マクロにしておく

	---- Const変数定義出力 ----
	DEF_GLCONST(name, value) マクロを定義のこと

	オプション:
	-a	出力ファイルに追記
	-d	definition出力
	-m	method定義出力
	-c	const変数定義出力
	(デフォルトは両方)
*/
#include "glextract.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <fstream>
const static int MAX_FILESIZE = 0x800000;
std::string ReadAll(std::istream& st) {
	auto pos = st.tellg();
	size_t sz = static_cast<size_t>(st.seekg(0, std::ios::end).tellg());
	st.seekg(0, std::ios::beg);
	if(sz > MAX_FILESIZE)
		throw std::runtime_error("too large input file");
	if(sz != 0) {
		std::string ret;
		ret.resize(sz);
		st.read(&ret[0], sz);

		st.seekg(pos);
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
	Exp_Const = 0x04,
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
					// Const変数出力
					case 'c':
						g_exp |= Exp_Const;
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
using StringSet = std::unordered_set<std::string>;
StringSet g_funcsInFile,
		  g_constsInFile;
void FindResource(std::fstream& fs) {
	std::string oBuff = ReadAll(fs);
	smatch res;
	auto fnAddName = [&oBuff, &res](auto& m, const auto& re, int idx){
		auto itr = oBuff.cbegin(),
			itrE = oBuff.cend();
		while(regex_search(itr, itrE, res, re)) {
			m.insert(res[idx].str());
			itr = res.suffix().first;
		}
	};
	fnAddName(g_funcsInFile, re_gldefine, 1);
	fnAddName(g_funcsInFile, re_gldefmethod, 1);
	fnAddName(g_constsInFile, re_gldefconst, 1);
}
struct CountNum {
	int count,
		skipcount;

	CountNum& operator += (const CountNum& c) {
		count += c.count;
		skipcount += c.skipcount;
		return *this;
	}
};
CountNum ExportConstValue(std::ostream& os, const std::string& buff) {
	CountNum c{};
	auto itr = buff.cbegin(),
		 itrE = buff.cend();
	smatch res;
	for(;;) {
		if(!regex_search(itr, itrE, res, re_define))
			break;
		itr = res.suffix().first;
		// 既に出力した定義名ならばスキップ
		const auto &name = res.str(1);
		if(g_constsInFile.count(name) == 0) {
			g_constsInFile.insert(name);
			const auto &value = res.str(2);
			os << "DEF_GLCONST(" << name << ", 0x" << value << ')' << std::endl;
			++c.count;
		} else
			++c.skipcount;
	}
	return c;
}
void ExportDefinition(std::ostream& os, const Func& func) {
	std::string funcDef;
	funcDef.resize(func.name.size());
	// :大文字変換
	std::transform(func.name.cbegin(), func.name.cend(), funcDef.begin(), ::toupper);
	std::stringstream ss;
	ss << "PFN" << funcDef << "PROC";
	// タイプ定義のアウトプット
	os << "GLDEFINE(" + func.name + ',' + ss.str() + ')' << std::endl;
}
void ExportMethod(std::ostream& os, const Func& func) {
	// ret_typeがvoidなら0, それ以外は1を出力
	int bVoid = (func.ret_type == "void") ? 0 : 1;
	os << "DEF_GLMETHOD(" << func.ret_type << ", " << bVoid << ", " << func.name << ", ";
	if(func.arg_pair.empty())
		os << "(), ()";
	else {
		if(func.arg_pair[0].type_name == "void")
			os << "(), ()";
		else {
			for(auto& ap : func.arg_pair)
				os << "(" << ap.type_name << ")";
			os << ", ";
			for(auto& ap : func.arg_pair)
				os << "(" << ap.name << ")";
		}
	}
	os << ')' << std::endl;
}
using ExportFunc = std::function<void (std::ostream&, const Func&)>;
using ExportFuncV = std::vector<ExportFunc>;
CountNum IterateFunctionDefinition(std::ostream& os, std::ifstream& def, const std::string& buff, const ExportFuncV& fl) {
	CountNum count{};
	smatch res;
	auto itr = buff.cbegin(),
		itrE = buff.cend();
	// 関数出力は別途用意したマクロ記述で範囲を制限
	std::string strMacro,
				strReBegin,
				strReEnd;
	for(;;) {
		if(def.eof())
			break;
		std::getline(def, strMacro);		// マクロ名
		std::getline(def, strReBegin);		// RegEx構文(Begin)
		std::getline(def, strReEnd);		// RegEx構文(End)
		if(strMacro.empty() || strReBegin.empty() || strReEnd.empty())
			break;
		regex re_begin(strReBegin),
				re_end(strReEnd);
		for(;;) {
			// 定義ファイルの記述に合った箇所まで読み飛ばし -> (読み込み始める地点)
			if(!regex_search(itr, itrE, res, re_begin))
				break;
			itr = res.suffix().first;

			// 定義の終わりを検出 -> (読み込み終わりの地点)
			if(!regex_search(itr, itrE, res, re_end)) {
				// 見つからない場合はエラー
				throw std::runtime_error("no matching definition of end of range");
			}

			bool bDef = true;
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
				if(g_bAppend && g_funcsInFile.count(func.name)!=0) {
					++count.skipcount;
					continue;
				}
				++count.count;
				if(bDef) {
					bDef = false;
					// マクロ名を書き出す
					os << "#ifdef " << strMacro << std::endl;
				}

				g_funcsInFile.insert(func.name);
				{
					auto itr = res[3].first,
						itrE = res[3].second;
					smatch res2;
					while(regex_search(itr, itrE, res2, re_args)) {
						func.arg_pair.push_back(ArgPair{res2.str(1), res2.str(2)});
						itr = res2.suffix().first;
					}
				}
				for(auto& f : fl)
					f(os, func);
			}
			if(!bDef)
				os << "#endif" << std::endl;
		}
	}
	return count;
}
int main(int argc, char* arg[]) {
	if(!CheckArgs(argc, arg)) {
		std::cout << "usage: glextract [-adm] [extraction definition file] [OpenGL Header(typically, glext.h)] [output filename]" << std::endl;
		return 1;
	}
	if(g_exp == 0)
		g_exp = Exp_All;
	try {
		// 入力ファイル内容を全部メモリにコピー
		std::ifstream ifs(g_arg[Arg_Input]);
		if(!ifs.is_open())
			throw std::runtime_error("can't open input-file");

		std::string buff = ReadAll(ifs);
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

		// 出力先ファイルに含まれる関数名を登録
		if(g_bAppend)
			FindResource(ofs);
		ofs.seekp(0, std::ios::end);

		CountNum count{};
		// ---------------- Const変数の検索と出力 ----------------
		if(g_exp & Exp_Const)
			count += ExportConstValue(ofs, buff);
		// ---------------- Definition, Method の検索と出力 ----------------
		ExportFuncV fl;
		if(g_exp & Exp_Definition) {
			// タイプ定義の名前を作成
			fl.emplace_back(&ExportDefinition);
		}
		if(g_exp & Exp_Method) {
			// GLラッパー定義のアウトプット
			fl.emplace_back(&ExportMethod);
		}
		if(!fl.empty())
			count += IterateFunctionDefinition(ofs, def, buff, fl);

		// スキップした数と出力された数を表示
		std::cout << count.count << " entries exported." << std::endl;
		std::cout << count.skipcount << " entries skipped." << std::endl;
	} catch(const std::exception& e) {
		std::cout << "error occured!" << std::endl << e.what() << std::endl;
	}
	return 0;
}
