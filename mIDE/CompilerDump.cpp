#include "Compiler.h"
#include "Resource.h"
#include <windows.h>
#include <richedit.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
	std::vector<HWND> FindEditors(HWND mainWindow)
	{
		std::vector<HWND> editors;
		EnumChildWindows(mainWindow, [](HWND hwnd, LPARAM parameter) -> BOOL
		{
			wchar_t className[64] = {};
			GetClassNameW(hwnd, className, ARRAYSIZE(className));
			if (wcscmp(className, MSFTEDIT_CLASS) == 0 ||
				wcscmp(className, L"RICHEDIT50W") == 0)
			{
				reinterpret_cast<std::vector<HWND>*>(parameter)->push_back(hwnd);
			}
			return TRUE;
		}, reinterpret_cast<LPARAM>(&editors));
		return editors;
	}
}

bool WriteInput(HWND editor, const std::wstring& source)
{
	return editor != nullptr && SetWindowTextW(editor, source.c_str()) != FALSE;
}

std::wstring ReadInput(HWND editor)
{
	if (!editor)
		return {};

	const int length = GetWindowTextLengthW(editor);
	std::wstring text(length + 1, L'\0');
	if (length > 0)
	{
		const int copied = GetWindowTextW(editor, text.data(), length + 1);
		text.resize(copied);
	}
	else
	{
		text.clear();
	}
	return text;
}

std::wstring ReadOutput(HWND outputEditor)
{
	return ReadInput(outputEditor);
}

int wmain(int argc, wchar_t** argv)
{
	const wchar_t* inputPath = argc > 1 ? argv[1] : L"CompilerSample.c";
	std::wifstream input(inputPath);
	if (!input)
	{
		std::wcerr << L"Could not open input file: " << inputPath << L"\n";
		return 1;
	}

	std::wstring source((std::istreambuf_iterator<wchar_t>(input)), {});
	HWND mainWindow = FindWindowW(nullptr, L"mIDE");
	if (!mainWindow)
	{
		std::wcerr << L"mIDE is not running\n";
		return 1;
	}

	const std::vector<HWND> editors = FindEditors(mainWindow);
	if (editors.size() < 2)
	{
		std::wcerr << L"Could not find both editor panes\n";
		return 1;
	}

	if (!WriteInput(editors[0], source) || ReadInput(editors[0]) != source)
	{
		std::wcerr << L"Failed to write or read the editor input\n";
		return 1;
	}

	SendMessageW(mainWindow, WM_COMMAND, MAKEWPARAM(IDM_COMPILE, 0), 0);
	Sleep(250);
	const std::wstring output = ReadOutput(editors[1]);
	std::wcerr << L"input_length=" << ReadInput(editors[0]).size()
		<< L" output_length=" << output.size() << L"\n";
	std::wcout << output;
	return 0;
}
