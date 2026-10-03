#include "CustomMenuBar.h"
#include <Uxtheme.h>
#include <vsstyle.h>
#pragma comment(lib, "uxtheme.lib")

static HHOOK g_hMenuHook = nullptr;
static CustomMenuBar* g_pCurrentBar = nullptr;
static HWND g_hCurrentWnd = nullptr;
static int g_nextMenuToOpen = -1;

// Hook for seamless hover-switching between menus while one is open
static LRESULT CALLBACK MenuMsgFilterHook(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == MSGF_MENU && g_pCurrentBar && g_hCurrentWnd)
    {
        MSG* pMsg = reinterpret_cast<MSG*>(lParam);
        if (pMsg->message == WM_MOUSEMOVE)
        {
            POINT pt = pMsg->pt;
            ScreenToClient(g_hCurrentWnd, &pt);

            int hit = g_pCurrentBar->HitTest(pt);
            if (hit != -1 && hit != g_pCurrentBar->HitTest(pt)) // switched item
            {
                g_nextMenuToOpen = hit;
                SendMessageW(g_hCurrentWnd, WM_CANCELMODE, 0, 0);
                return 0;
            }
        }
    }
    return CallNextHookEx(g_hMenuHook, nCode, wParam, lParam);
}

CustomMenuBar::CustomMenuBar()
{
    m_brItemHot = CreateSolidBrush(RGB(0x26, 0x26, 0x26));
    m_brItemActive = CreateSolidBrush(RGB(0x33, 0x33, 0x33));
    m_brItemBorder = CreateSolidBrush(RGB(0x3E, 0x3E, 0x3E));
}

CustomMenuBar::~CustomMenuBar()
{
    if (m_hLoadedRootMenu)
    {
        DestroyMenu(m_hLoadedRootMenu);
        m_hLoadedRootMenu = nullptr;
    }
    DeleteObject(m_brItemHot);
    DeleteObject(m_brItemActive);
    DeleteObject(m_brItemBorder);
}

bool CustomMenuBar::LoadFromResource(HINSTANCE hInstance, LPCWSTR lpMenuName)
{
    HINSTANCE hInst = hInstance ? hInstance : GetModuleHandleW(NULL);
    m_hLoadedRootMenu = LoadMenuW(hInst, lpMenuName);
    if (!m_hLoadedRootMenu)
        return false;

    m_items.clear();
    int count = GetMenuItemCount(m_hLoadedRootMenu);

    for (int i = 0; i < count; ++i)
    {
        wchar_t labelBuffer[256] = { 0 };
        MENUITEMINFOW mii = { sizeof(mii) };
        mii.fMask = MIIM_STRING | MIIM_SUBMENU;
        mii.dwTypeData = labelBuffer;
        mii.cch = 255;

        if (GetMenuItemInfoW(m_hLoadedRootMenu, i, TRUE, &mii) && mii.hSubMenu && mii.cch > 0)
        {
            CustomMenuEntry entry;
            entry.label = labelBuffer;
            entry.hSubMenu = mii.hSubMenu;

            std::wstring clean;
            for (size_t k = 0; k < entry.label.size(); ++k)
            {
                if (entry.label[k] == L'&' && k + 1 < entry.label.size())
                {
                    entry.accelerator = towupper(entry.label[k + 1]);
                    continue;
                }
                clean += entry.label[k];
            }
            entry.cleanLabel = clean;
            m_items.push_back(entry);
        }
    }
    return !m_items.empty();
}

bool CustomMenuBar::AppendMenuItem(
    const std::wstring& parentLabel, UINT commandId, const std::wstring& label)
{
    if (!m_hLoadedRootMenu)
        return false;

    const int count = GetMenuItemCount(m_hLoadedRootMenu);
    for (int i = 0; i < count; ++i)
    {
        wchar_t labelBuffer[256] = {};
        MENUITEMINFOW mii = { sizeof(mii) };
        mii.fMask = MIIM_STRING | MIIM_SUBMENU;
        mii.dwTypeData = labelBuffer;
        mii.cch = ARRAYSIZE(labelBuffer) - 1;
        if (!GetMenuItemInfoW(m_hLoadedRootMenu, i, TRUE, &mii) || !mii.hSubMenu)
            continue;

        std::wstring cleanLabel;
        for (size_t k = 0; k < wcslen(labelBuffer); ++k)
        {
            if (labelBuffer[k] != L'&')
                cleanLabel += labelBuffer[k];
        }

        if (cleanLabel == parentLabel)
            return AppendMenuW(mii.hSubMenu, MF_STRING, commandId, label.c_str()) != FALSE;
    }

    return false;
}

void CustomMenuBar::DrawInline(HTHEME hTheme, HDC hdcPaint, int startX, int startY, int height,
    HFONT hFont, bool isFocused, COLORREF textColor)
{
    if (m_items.empty())
    {
        CustomMenuEntry f; f.label = L"&File"; f.cleanLabel = L"File"; f.accelerator = 'F';
        CustomMenuEntry h; h.label = L"&Help"; h.cleanLabel = L"Help"; h.accelerator = 'H';
        m_items.push_back(f);
        m_items.push_back(h);
    }

    HFONT oldFont = hFont ? (HFONT)SelectObject(hdcPaint, hFont) : nullptr;

    extern DWORD g_buildNumber; // Access your existing build number global
	extern bool g_darkModeEnabled; // Access your existing dark mode global
    const bool useDrawTextW = (g_buildNumber >= 9200);

    DTTOPTS opts = { sizeof(opts) };
    opts.dwFlags = DTT_COMPOSITED | DTT_TEXTCOLOR;
    opts.crText = textColor;

    int curX = startX;
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        auto& item = m_items[i];

        SIZE sz = { 0, 0 };
        GetTextExtentPoint32W(hdcPaint, item.cleanLabel.c_str(), (int)item.cleanLabel.size(), &sz);
        int itemWidth = sz.cx + 14;
        item.rc = { curX, startY, curX + itemWidth, startY + height };
        RECT borderRc = item.rc; borderRc.bottom -= 1;

        bool isActive = ((int)i == m_activeIndex);
        bool isHot = ((int)i == m_hotIndex);

        if (isActive)
        {
            FillRect(hdcPaint, &item.rc, m_brItemActive);
            FrameRect(hdcPaint, &borderRc, m_brItemBorder);
        }
        else if (isHot)
        {
            FillRect(hdcPaint, &item.rc, m_brItemHot);
            FrameRect(hdcPaint, &borderRc, m_brItemBorder);
        }

        // --- TEXT RENDERING BRANCH ---
        if (useDrawTextW)
        {
            // Windows 8+ : Crisp ClearType directly over the pre-filled dark background!
            COLORREF bgColor = isActive
                ? RGB(0x33, 0x33, 0x33)
                : isHot
                    ? RGB(0x26, 0x26, 0x26)
                    : (g_darkModeEnabled
                        ? (isFocused ? RGB(0, 0, 0) : RGB(43, 43, 43))
                        : RGB(255, 255, 255));
            SetBkColor(hdcPaint, bgColor);
            SetBkMode(hdcPaint, OPAQUE);
            SetTextColor(hdcPaint, textColor);
            DrawTextW(hdcPaint, item.label.c_str(), -1, &item.rc,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        else if (hTheme)
        {
            // Windows Vista / 7 : DWM alpha compositing
            DrawThemeTextEx(hTheme, hdcPaint, WP_CAPTION,
                isFocused ? CS_ACTIVE : CS_INACTIVE,
                item.label.c_str(), -1,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE,
                &item.rc, &opts);
        }

        curX += itemWidth + 2;
    }

    if (oldFont)
        SelectObject(hdcPaint, oldFont);
}

int CustomMenuBar::HitTest(POINT ptClient) const
{
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        if (PtInRect(&m_items[i].rc, ptClient))
            return (int)i;
    }
    return -1;
}

void CustomMenuBar::TrackLeave(HWND hWnd)
{
    if (!m_trackingMouse)
    {
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hWnd, 0 };
        TrackMouseEvent(&tme);
        m_trackingMouse = true;
    }
}

void CustomMenuBar::InvalidateItem(HWND hWnd, int index)
{
    if (index >= 0 && index < (int)m_items.size())
        InvalidateRect(hWnd, &m_items[index].rc, TRUE);
}

bool CustomMenuBar::OnMouseMove(HWND hWnd, POINT pt)
{
    TrackLeave(hWnd);
    int hit = HitTest(pt);

    if (hit != m_hotIndex)
    {
        InvalidateItem(hWnd, m_hotIndex);
        m_hotIndex = hit;
        InvalidateItem(hWnd, m_hotIndex);
    }
    return (hit != -1);
}

bool CustomMenuBar::OnMouseLeave(HWND hWnd)
{
    m_trackingMouse = false;
    if (m_hotIndex != -1)
    {
        InvalidateItem(hWnd, m_hotIndex);
        m_hotIndex = -1;
    }
    return true;
}

bool CustomMenuBar::OnLButtonDown(HWND hWnd, POINT pt)
{
    int hit = HitTest(pt);
    if (hit != -1)
    {
        OpenMenu(hWnd, hit);
        return true;
    }
    return false;
}

bool CustomMenuBar::OnSysCommand(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    if ((wParam & 0xFFF0) == SC_KEYMENU && lParam != 0)
    {
        wchar_t key = towupper((wchar_t)lParam);
        for (size_t i = 0; i < m_items.size(); ++i)
        {
            if (m_items[i].accelerator == key)
            {
                OpenMenu(hWnd, (int)i);
                return true;
            }
        }
    }
    return false;
}

void CustomMenuBar::OpenMenu(HWND hWnd, int index)
{
    g_nextMenuToOpen = -1;

    while (index >= 0 && index < (int)m_items.size())
    {
        m_activeIndex = index;
        InvalidateItem(hWnd, index);

        POINT pt = { m_items[index].rc.left, m_items[index].rc.bottom };
        ClientToScreen(hWnd, &pt);

        g_pCurrentBar = this;
        g_hCurrentWnd = hWnd;
        g_hMenuHook = SetWindowsHookExW(WH_MSGFILTER, MenuMsgFilterHook, nullptr, GetCurrentThreadId());

        TrackPopupMenuEx(
            m_items[index].hSubMenu,
            TPM_LEFTALIGN | TPM_TOPALIGN | TPM_VERNEGANIMATION,
            pt.x, pt.y,
            hWnd,
            nullptr
        );

        UnhookWindowsHookEx(g_hMenuHook);
        g_hMenuHook = nullptr;
        g_pCurrentBar = nullptr;

        InvalidateItem(hWnd, index);

        index = g_nextMenuToOpen;
        g_nextMenuToOpen = -1;
    }

    m_activeIndex = -1;
    m_hotIndex = -1;
    InvalidateRect(hWnd, nullptr, TRUE);
}