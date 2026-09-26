#pragma once

enum ThemeColorIndex {
	CLR_WINDOW_BG = 0,
	CLR_WINDOW_TEXT,
	CLR_GUTTER_BG,
	CLR_GUTTER_TEXT,
	CLR_CODE_BG,
	CLR_CODE_TEXT,
	// later ill add red, blue, and whatnot color highlights
	CLR_COUNT
};

extern COLORREF	g_ThemeColors[CLR_COUNT];
extern HBRUSH	g_ThemeBrushes[CLR_COUNT];
extern bool		g_ThemeInitialized;
extern bool		g_ThemeMode; // true on dark/high contrast, false otherwise

void InitializeTheme();
void UpdateThemePalette();
void CleanupTheme();
bool IsDarkMode();
bool IsHighContrast();

// helpers
void ApplyThemeToRichEdit(HWND hwndRichEdit);
