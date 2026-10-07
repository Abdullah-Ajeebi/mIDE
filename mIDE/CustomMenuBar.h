#pragma once
#include <windows.h>
#include <uxtheme.h>
#include <string>
#include <vector>

struct CustomMenuEntry
{
    std::wstring label;
    std::wstring cleanLabel;
    wchar_t accelerator = 0;
    HMENU hSubMenu = nullptr;
    RECT rc = { 0, 0, 0, 0 };
};

class CustomMenuBar
{
public:
    CustomMenuBar();
    ~CustomMenuBar();

    // Accepts string resource names OR MAKEINTRESOURCEW(...)
    bool LoadFromResource(HINSTANCE hInstance, LPCWSTR lpMenuName);
    bool AppendMenuItem(const std::wstring& parentLabel, UINT commandId,
        const std::wstring& label);

    // Inline helper overload if you pass a raw integer ID like IDR_MAINMENU:
    bool LoadFromResource(HINSTANCE hInstance, UINT uMenuResId) {
        return LoadFromResource(hInstance, MAKEINTRESOURCEW(uMenuResId));
    }

    // 2. Draw directly into PaintCustomCaption's hdcPaint!
    void DrawInline(HTHEME hTheme, HDC hdcPaint, int startX, int startY, int height,
        HFONT hFont, bool isFocused, COLORREF textColor, bool useDrawTextW);

    // 3. Message Handlers
    int HitTest(POINT ptClient) const;
    bool OnMouseMove(HWND hWnd, POINT ptClient);
    bool OnMouseLeave(HWND hWnd);
    bool OnLButtonDown(HWND hWnd, POINT ptClient);
    bool OnSysCommand(HWND hWnd, WPARAM wParam, LPARAM lParam);

    void OpenMenu(HWND hWnd, int index);

private:
    std::vector<CustomMenuEntry> m_items;
    HMENU m_hLoadedRootMenu = nullptr;
    int m_hotIndex = -1;
    int m_activeIndex = -1;
    bool m_trackingMouse = false;

    HBRUSH m_brItemHot;
    HBRUSH m_brItemActive;
    HBRUSH m_brItemBorder;

    void TrackLeave(HWND hWnd);
    void InvalidateItem(HWND hWnd, int index);
};