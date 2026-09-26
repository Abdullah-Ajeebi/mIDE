#include "Thememan.h"

COLORREF	g_ThemeColors[CLR_COUNT] = { 0 };
HBRUSH		g_ThemeBrushes[CLR_COUNT] = { NULL };
bool		g_ThemeInitialized = false;
bool		g_ThemeMode = false;

void UpdateThemePalette() {
	g_ThemeMode = IsHighContrast() || IsDarkMode();
	if (g_ThemeMode) {
		g_ThemeColors[CLR_WINDOW_BG]	= RGB(30, 30, 30);
		g_ThemeColors[CLR_WINDOW_TEXT]	= RGB(220, 220, 220);
		g_ThemeColors[CLR_GUTTER_BG]	= RGB(43, 43, 43);
		g_ThemeColors[CLR_GUTTER_TEXT]	= RGB(200, 200, 200);
		g_ThemeColors[CLR_CODE_BG]		= RGB(40, 40, 40);
		g_ThemeColors[CLR_CODE_TEXT]	= RGB(230, 230, 230);
	}
	else {
		g_ThemeColors[CLR_WINDOW_BG]	= RGB(255, 255, 255);
		g_ThemeColors[CLR_WINDOW_TEXT]	= RGB(0, 0, 0);
		g_ThemeColors[CLR_GUTTER_BG]	= RGB(240, 240, 240);
		g_ThemeColors[CLR_GUTTER_TEXT]	= RGB(50, 50, 50);
		g_ThemeColors[CLR_CODE_BG]		= RGB(232, 232, 232);
		g_ThemeColors[CLR_CODE_TEXT]	= RGB(0, 0, 0);
	}

	for (int i = 0; i < CLR_COUNT; ++i) {
		if (g_ThemeBrushes[i]) {
			DeleteObject(g_ThemeBrushes[i]);
			g_ThemeBrushes[i] = NULL;
		}
		g_ThemeBrushes[i] = CreateSolidBrush(g_ThemeColors[i]);
	}
}

void InitializeTheme() {
	UpdateThemePalette();
	g_ThemeInitialized = true;
}

void CleanupTheme() {
	for (int i = 0; i < CLR_COUNT; ++i) {
		if (g_ThemeBrushes[i] != NULL) {
			DeleteObject(g_ThemeBrushes[i]);
			g_ThemeBrushes[i] = NULL;
		}
	}
	g_ThemeInitialized = false;
}

bool IsDarkMode() {
	HKEY hKey;
	LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey);
	
	if (lRes == ERROR_SUCCESS) {
		DWORD dwValue = 0;
		DWORD dwSize = sizeof(dwValue);
		lRes = RegQueryValueExW(hKey, L"AppsUseLightTheme", 0, NULL, (LPBYTE)&dwValue, &dwSize);
		RegCloseKey(hKey);

		if (lRes == ERROR_SUCCESS) {
			return dwValue == 0;
		}
	}
	return false;
}

void ApplyThemeToRichEdit(HWND hRichEdit) {
	if (!hRichEdit) return;

	::SendMessageW(hRichEdit, EM_SETBKGNDCOLOR, 0, (LPARAM)g_ThemeColors[CLR_CODE_BG]);

	CHARFORMAT2W cf = { 0 };
	ZeroMemory(&cf, sizeof(cf));
	cf.cbSize = sizeof(CHARFORMAT2W);

	cf.dwMask = CFM_COLOR | CFM_EFFECTS;
	cf.crTextColor = g_ThemeColors[CLR_CODE_TEXT];

	SetWindowTheme(hRichEdit, L"DarkMode_Explorer", nullptr);

	cf.dwEffects = 0;

	::SendMessageW(hRichEdit, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
	::SendMessageW(hRichEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
}
