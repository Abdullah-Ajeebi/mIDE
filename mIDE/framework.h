// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#include <Richedit.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#include <Uxtheme.h>
#pragma comment(lib, "UxTheme.lib")
#include <WindowsX.h>
#include <Vssym32.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
// C++ Standard Library
#include <set>
#include <thread>
