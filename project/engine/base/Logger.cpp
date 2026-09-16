#include "Logger.h"

namespace Logger {
	void Log(const std::string& message) {
		OutputDebugStringA(message.c_str());
	}
	void Log(std::ostream& os, const std::string& message) {
		// ファイル出力などを残しつつ、デバッガーでも同じ内容を確認できるようにする。
		os << message << std::endl;
		OutputDebugStringA(message.c_str());
	}
}
