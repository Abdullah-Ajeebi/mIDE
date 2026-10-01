#pragma once

#include <string>

struct CompileResult
{
	bool success = false;
	std::wstring output;
	std::wstring error;
	int errorLine = 0;
};

CompileResult CompileCToMlog(const std::wstring& source, bool optimize = true);
