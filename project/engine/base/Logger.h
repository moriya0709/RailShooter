#pragma once
#include <Windows.h>
#include <string>
#include <debugapi.h>
#include <iostream>

namespace Logger {
	// Visual Studio のデバッグ出力へ書き込む。
	void Log(const std::string& message);
	// 任意のストリームとデバッグ出力の両方へ同じメッセージを送る。
	void Log(std::ostream& os, const std::string& message);

};
