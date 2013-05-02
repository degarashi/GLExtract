/*! glext.h から関数宣言を抜き出してファイルに書き出すプログラム
	戻り値は手抜き: 0が正常終了, 1が異常終了
	標準入力にglext.hを, 出力は標準出力 (glfunc.inc などのファイル名を与える)
	
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
#ifdef USE_BOOST
	#include <boost/regex.hpp>
	const boost::regex re_proto("^GLAPI\\s+void\\s+APIENTRY\\s+([a-zA-Z0-9_]+)"),
			re_endif("^#endif");
	int main() {
		using boost::regex;
		using boost::smatch;
		using boost::regex_search;
#else
	#include <regex>
	const std::regex re_proto("^GLAPI\\s+void\\s+APIENTRY\\s+(\\[a-zA-Z0-9_\\]+)"),
			re_endif("^#endif");
	int main() {
		using std::regex;
		using std::std::smatch;
		using std::regex_search;
#endif
	// ファイル内容を全部メモリにコピー
	size_t sz = static_cast<size_t>(std::cin.seekg(0, std::ios::end).tellg());
	if(sz == 0)
		return 1;
	std::string buff;
	buff.resize(sz);
	std::cin.seekg(0, std::ios::beg).read(&buff[0], sz);
	
	auto itr = buff.cbegin(),
		itrE = buff.cend();
	// OpenGL1.2以降の定義が対象
	const char* c_version[] = {
		"1_[2-9]\\d*",
		"2_\\d+",
		"3_\\d+",
		"4_\\d+"
	};
	smatch res;
	for(auto& cv : c_version) {
		std::stringstream ss;
		ss << "^#define\\s+GL_VERSION_" << cv << "\\s+1";
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
				std::cout << "GLDEFINE(" + funcName + ',' + ss.str() + ')' << std::endl;
			}
		}
	}
	return 0;
}
