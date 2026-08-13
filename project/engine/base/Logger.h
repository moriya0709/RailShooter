#pragma once
#include <Windows.h>
#include <string>
#include <debugapi.h>
#include <iostream>

namespace Logger {
	void Log(const std::string& message);
	void Log(std::ostream& os, const std::string& message);

};

