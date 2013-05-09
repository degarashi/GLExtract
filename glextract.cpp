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

#ifdef USE_BOOST
	#include <boost/regex.hpp>
	const boost::regex re_proto("^GLAPI\\s+(?:[a-zA-Z0-9_]+)\\s+APIENTRY\\s+([a-zA-Z0-9_]+)"),
			re_endif("^#endif");
	int main(int argc, char* arg[]) {
		using boost::regex;
		using boost::smatch;
		using boost::regex_search;
#else
	#include <regex>
	const std::regex re_proto("^GLAPI\\s+(?:\\[a-zA-Z0-9_\\]+)\\s+APIENTRY\\s+(\\[a-zA-Z0-9_\\]+)"),
			re_endif("^#endif");
	int main(int argc, char* arg[]) {
		using std::regex;
		using std::std::smatch;
		using std::regex_search;
#endif
	if(argc != 3) {
		std::cout << "usage: glextract [OpenGL Header(typically, glext.h)] [output filename]" << std::endl;
		return 0;
	}
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
					ss.str("");
					std::string funcName = res[1].str(),
								funcDef;
					funcDef.resize(funcName.size());
					// タイプ定義の名前を作成
					// :大文字変換
					std::transform(funcName.cbegin(), funcName.cend(), funcDef.begin(), ::toupper);
					ss << "PFN" << funcDef << "PROC";
					// タイプ定義のアウトプット
					ofs << "GLDEFINE(" + funcName + ',' + ss.str() + ')' << std::endl;
				}
			}
		}
	} catch(const std::exception& e) {
		std::cout << "error occured!" << std::endl << e.what() << std::endl;
	}
	return 0;
}
