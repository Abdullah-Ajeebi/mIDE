// mIDE.cpp : Defines the entry point for the application.

#include "framework.h"
#include "mIDE.h"
#include "Thememan.h"
#include "Thememan.cpp"
#include "Compiler.h"
#include "GutteredTextEditor.cpp"
#include "DarkMode.h"
#include "CustomMenuBar.h"
#include <vsstyle.h>
#include <vssym32.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shlobj.h>
#include <string>
#include <fstream>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")

#define MAX_LOADSTRING 100

#define STARTUPSPEED

// --- TCP CLIENT CLASS ---
class MideDebuggerClient {
    SOCKET connectSocket = INVALID_SOCKET;

public:
    MideDebuggerClient() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    ~MideDebuggerClient() {
        disconnect();
        WSACleanup();
    }

    bool connectToServer(const char* ip, const char* port, DWORD timeoutMs = 2000) {
        if (connectSocket != INVALID_SOCKET) return true;

        struct addrinfo hints = { 0 }, * result = NULL;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        if (getaddrinfo(ip, port, &hints, &result) != 0) return false;

        connectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

        setsockopt(connectSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));
        setsockopt(connectSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

        if (connect(connectSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
            disconnect();
            freeaddrinfo(result);
            return false;
        }
        freeaddrinfo(result);
        return true;
    }

    void disconnect() {
        if (connectSocket != INVALID_SOCKET) {
            closesocket(connectSocket);
            connectSocket = INVALID_SOCKET;
        }
    }

    std::string sendCommand(const std::string& command) {
        if (connectSocket == INVALID_SOCKET) return "ERROR: Not connected";

        std::string payload = command + "\n";
        send(connectSocket, payload.c_str(), (int)payload.length(), 0);

        std::string fullResponse = "";
        char recvbuf[4096];

        while (true) {
            int bytesReceived = recv(connectSocket, recvbuf, sizeof(recvbuf) - 1, 0);

            if (bytesReceived > 0) {
                recvbuf[bytesReceived] = '\0';
                fullResponse += recvbuf;

                if (fullResponse.find("END\n") != std::string::npos ||
                    fullResponse.find("END\r") != std::string::npos) {
                    break;
                }
            }
            else if (bytesReceived == 0) {
                return fullResponse + "\n[Server Disconnected]";
            }
            else {
                break;
            }
        }

        size_t endPos = fullResponse.rfind("END");
        if (endPos != std::string::npos) {
            fullResponse.erase(endPos);
        }

        return fullResponse;
    }
};

std::wstring s2ws(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string ws2s(const std::wstring& str) {
    if (str.empty()) return std::string();
    int sizeNeeded = WideCharToMultiByte(
        CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(sizeNeeded, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, str.c_str(), -1, result.data(), sizeNeeded, nullptr, nullptr);
    result.resize(sizeNeeded > 0 ? sizeNeeded - 1 : 0);
    return result;
}

constexpr int LEFTEXTENDWIDTH = 8;
constexpr int TOPEXTENDWIDTH = 30;
constexpr int RIGHTEXTENDWIDTH = 8;
constexpr int BOTTOMEXTENDWIDTH = 8;

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
GutteredTextEditor* editor;
GutteredTextEditor* compiledEditor;
CustomMenuBar g_menuBar;
static bool g_isWindowActive = true;
static HWND g_splitter = nullptr;
static int g_splitterX = 0;
static bool g_draggingSplitter = false;
constexpr int SPLITTER_WIDTH = 6;
MideDebuggerClient* g_debuggerClient = nullptr;
#ifdef STARTUPSPEED
LARGE_INTEGER qpcFreq, qpcStart;
#endif

// Persisted application settings stored per-user under %APPDATA%\mIDE.
struct AppSettings {
    std::wstring host = L"127.0.0.1";
    int port = 9999;
    int timeoutMs = 2000;
    int speedIndex = 0; // placeholder
    bool optimizerEnabled = true;
    bool linterEnabled = true;
    bool linterWarnings = true;
    int cmixProfile = 0;
    int cmixBlockSize = 64;
};
static AppSettings g_settings;

static std::wstring LoadEmbeddedCompilerSample(HINSTANCE hInstance)
{
    HRSRC resource = FindResourceW(
        hInstance,
        MAKEINTRESOURCEW(IDR_COMPILER_SAMPLE),
        RT_RCDATA);
    if (!resource)
        return {};

    HGLOBAL loaded = LoadResource(hInstance, resource);
    const DWORD size = SizeofResource(hInstance, resource);
    if (!loaded || size == 0)
        return {};

    const char* bytes = static_cast<const char*>(LockResource(loaded));
    if (!bytes)
        return {};

    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, bytes, static_cast<int>(size), nullptr, 0);
    if (required <= 0)
        return {};

    std::wstring sample(required, L'\0');
    if (MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, bytes, static_cast<int>(size),
        sample.data(), required) != required)
        return {};

    return sample;
}

static std::wstring SettingsFilePath()
{
    wchar_t appDataPath[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appDataPath)))
        return {};

    std::wstring directory = std::wstring(appDataPath) + L"\\mIDE";
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return {};

    return directory + L"\\settings.txt";
}

static void SaveSettingsToFile()
{
    const std::wstring path = SettingsFilePath();
    if (path.empty())
        return;

    std::wofstream f(path);
    if (!f) return;
    f << g_settings.host << L"\n" << g_settings.port << L"\n" << g_settings.timeoutMs << L"\n"
      << g_settings.speedIndex << L"\n" << g_settings.optimizerEnabled << L"\n"
      << g_settings.linterEnabled << L"\n" << g_settings.linterWarnings << L"\n"
      << g_settings.cmixProfile << L"\n" << g_settings.cmixBlockSize << L"\n";
}

static void LoadSettingsFromFile()
{
    const std::wstring path = SettingsFilePath();
    if (path.empty())
        return;

    std::wifstream f(path);
    if (!f) return;
    std::wstring host; int port; int timeout; int speed;
    if (!(f >> host)) return;
    if (!(f >> port)) return;
    if (!(f >> timeout)) return;
    if (!(f >> speed)) return;
    g_settings.host = host;
    g_settings.port = port;
    g_settings.timeoutMs = timeout;
    g_settings.speedIndex = speed;
    f >> g_settings.optimizerEnabled >> g_settings.linterEnabled
      >> g_settings.linterWarnings >> g_settings.cmixProfile
      >> g_settings.cmixBlockSize;
}

// Message used to receive test-connect result posted from worker thread
#define WM_TESTCONNECT_RESULT (WM_APP + 50)

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    Trollge(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    SettingsDlgProc(HWND, UINT, WPARAM, LPARAM);
void                ResizeEditorPanes(HWND hWnd, int width, int height);

LRESULT CALLBACK SplitterProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
        return TRUE;
    case WM_LBUTTONDOWN:
        g_draggingSplitter = true;
        SetCapture(hwnd);
        return 0;
    case WM_MOUSEMOVE:
        if (g_draggingSplitter)
        {
            POINT point = {};
            GetCursorPos(&point);
            HWND parent = GetParent(hwnd);
            ScreenToClient(parent, &point);
            RECT client = {};
            GetClientRect(parent, &client);
            g_splitterX = max(120, min(point.x, client.right - 120 - SPLITTER_WIDTH));
            ResizeEditorPanes(parent, client.right, client.bottom);
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_draggingSplitter)
        {
            g_draggingSplitter = false;
            ReleaseCapture();
        }
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps = {};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT client = {};
        GetClientRect(hwnd, &client);
        FillRect(hdc, &client, g_ThemeBrushes[CLR_GUTTER_BG]);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

ATOM RegisterSplitterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SplitterProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_SIZEWE);
    wc.hbrBackground = g_ThemeBrushes[CLR_GUTTER_BG];
    wc.lpszClassName = L"mIDESplitter";
    return RegisterClassExW(&wc);
}

void ApplyDarkModeToWindow(HWND hWnd)
{
    if (!g_darkModeSupported)
        return;

    AllowDarkModeForWindow(hWnd, g_darkModeEnabled);
    RefreshTitleBarThemeColor(hWnd);
}

void PaintCustomCaption(HWND hWnd, HDC hdc)
{
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    HTHEME hTheme = OpenThemeData(hWnd, L"WINDOW");
    if (!hTheme)
        return;

    HDC hdcPaint = CreateCompatibleDC(hdc);
    if (!hdcPaint)
    {
        CloseThemeData(hTheme);
        return;
    }

    const int width = rcClient.right - rcClient.left;
    const int height = rcClient.bottom - rcClient.top;
    const int captionButtonWidth = g_buildNumber >= 10240
        ? 46 * 3 - 1
        : 50 + 25 + 25;
    const int captionButtonHeight = 30 + (IsZoomed(hWnd) ? 8 : 0);
    const int topBarHeight = captionButtonHeight - (IsZoomed(hWnd) ? 8 : 0);
    BITMAPINFO dib = {};
    dib.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    dib.bmiHeader.biWidth = width;
    dib.bmiHeader.biHeight = -topBarHeight;
    dib.bmiHeader.biPlanes = 1;
    dib.bmiHeader.biBitCount = 32;
    dib.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdc, &dib, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (hBitmap)
    {
        COLORREF bgColor = g_darkModeEnabled
            ? (g_isWindowActive ? RGB(0, 0, 0) : RGB(43, 43, 43))
            : RGB(255, 255, 255);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(hdcPaint, hBitmap);
        HICON smallIcon = (HICON)GetClassLongPtrW(hWnd, GCLP_HICONSM);
        if (smallIcon)
        {
            const int iconSize = GetSystemMetrics(SM_CXSMICON);
            DrawIconEx(hdcPaint, 8, 7, smallIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
        }

        LOGFONTW logFont = {};
        HFONT oldFont = nullptr;
        HFONT captionFont = nullptr;
        if (SUCCEEDED(GetThemeSysFont(hTheme, TMT_CAPTIONFONT, &logFont)))
        {
            captionFont = CreateFontIndirectW(&logFont);
            oldFont = (HFONT)SelectObject(hdcPaint, captionFont);
        }

        WCHAR title[256] = {};
        GetWindowTextW(hWnd, title, ARRAYSIZE(title));
        DTTOPTS options = { sizeof(options) };
        options.dwFlags = DTT_COMPOSITED | DTT_TEXTCOLOR;
        options.crText = g_darkModeEnabled
            ? (g_isWindowActive ? RGB(255, 255, 255) : RGB(150, 150, 150))
            : (g_isWindowActive ? RGB(0, 0, 0) : RGB(153, 153, 153));

        SIZE szTitle = { 0, 0 };
        GetTextExtentPoint32W(hdcPaint, title, (int)wcslen(title), &szTitle);

        RECT titleRect = rcClient;
        titleRect.top += (IsZoomed(hWnd) ? 8 : 0);
        titleRect.left += 29;
        titleRect.right -= captionButtonWidth + szTitle.cx;
        titleRect.bottom -= height - (captionButtonHeight - (IsZoomed(hWnd) ? 8 : 0));
        if (titleRect.right < titleRect.left)
            titleRect.right = titleRect.left;
        if (titleRect.bottom < titleRect.top)
            titleRect.bottom = titleRect.top;
        const bool useDrawTextW = (g_buildNumber >= 9200);

        if (useDrawTextW)
        {
            SetBkColor(hdcPaint, bgColor);
            SetBkMode(hdcPaint, TRANSPARENT);
            SetTextColor(hdcPaint, options.crText);
            DrawTextW(hdcPaint, title, -1, &titleRect,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
        else
        {
            DrawThemeTextEx(hTheme, hdcPaint, WP_CAPTION,
                g_isWindowActive ? CS_ACTIVE : CS_INACTIVE, title, -1,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS,
                &titleRect, &options);
        }

        int menuStartX = titleRect.left + szTitle.cx + 16;

        g_menuBar.DrawInline(
            hTheme,
            hdcPaint,
            menuStartX,
            3,
            captionButtonHeight - 6,
            captionFont,
            g_isWindowActive,
            options.crText
        );

        DWORD* pPixels = reinterpret_cast<DWORD*>(pixels);
        const int totalPixels = width * topBarHeight;

        for (int i = 0; i < totalPixels; ++i)
        {
            DWORD rgb = pPixels[i] & 0x00FFFFFF;

            if (rgb == 0x00000000 || rgb == bgColor)
            {
                pPixels[i] = 0xFF000000 | bgColor;
            }
            else
            {
                pPixels[i] |= 0xFF000000;
            }
        }

        BitBlt(hdc, 0, 1, width - captionButtonWidth, topBarHeight - (IsZoomed(hWnd) ? -6 : 1), hdcPaint, 0, 0, SRCCOPY);
        SelectObject(hdcPaint, oldBitmap);
        if (oldFont)
            SelectObject(hdcPaint, oldFont);
        if (captionFont)
            DeleteObject(captionFont);
        DeleteObject(hBitmap);
    }

    DeleteDC(hdcPaint);
    CloseThemeData(hTheme);
}

LRESULT HitTestNCA(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    POINT ptMouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
    POINT ptClient = ptMouse;
    ScreenToClient(hWnd, &ptClient);

    if (g_menuBar.HitTest(ptClient) != -1)
    {
        return HTCLIENT;
    }

    RECT rcWindow;
    GetWindowRect(hWnd, &rcWindow);

    RECT rcFrame = { 0 };
    AdjustWindowRectEx(&rcFrame, WS_OVERLAPPEDWINDOW & ~WS_CAPTION, FALSE, NULL);

    USHORT uRow = 1;
    USHORT uCol = 1;
    bool fOnResizeBorder = false;

    if (ptMouse.y >= rcWindow.top && ptMouse.y < rcWindow.top + TOPEXTENDWIDTH)
    {
        fOnResizeBorder = (ptMouse.y < (rcWindow.top - rcFrame.top));
        uRow = 0;
    }
    else if (ptMouse.y < rcWindow.bottom && ptMouse.y >= rcWindow.bottom - BOTTOMEXTENDWIDTH)
    {
        uRow = 2;
    }

    if (ptMouse.x >= rcWindow.left && ptMouse.x < rcWindow.left + LEFTEXTENDWIDTH)
    {
        uCol = 0;
    }
    else if (ptMouse.x < rcWindow.right && ptMouse.x >= rcWindow.right - RIGHTEXTENDWIDTH)
    {
        uCol = 2;
    }

    LRESULT hitTests[3][3] =
    {
        { HTTOPLEFT,    fOnResizeBorder ? HTTOP : HTCAPTION,    HTTOPRIGHT },
        { HTLEFT,       HTNOWHERE,                             HTRIGHT },
        { HTBOTTOMLEFT, HTBOTTOM,                              HTBOTTOMRIGHT },
    };

    return hitTests[uRow][uCol];
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

#ifdef STARTUPSPEED
    QueryPerformanceFrequency(&qpcFreq);
    QueryPerformanceCounter(&qpcStart);
#endif

    INITCOMMONCONTROLSEX commonControls = {};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES;
    if (!InitCommonControlsEx(&commonControls))
        return FALSE;

    InitDarkMode();
    InitializeTheme();

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MIDE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);
    RegisterSplitterClass(hInstance);
    GutteredTextEditor::RegisterWindowClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MIDE));
    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MIDE));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = g_ThemeBrushes[CLR_WINDOW_BG];
    wcex.lpszMenuName = 0;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

void ResizeEditorPanes(HWND hWnd, int width, int height)
{
    if (!editor || !compiledEditor || !g_splitter)
        return;

    const int top = TOPEXTENDWIDTH;
    const int paneHeight = max(0, height - top);
    const int maxSplitterX = max(120, width - 120 - SPLITTER_WIDTH);
    g_splitterX = max(120, min(g_splitterX, maxSplitterX));

    MoveWindow(editor->GetHWND(), 0, top, g_splitterX, paneHeight, TRUE);
    MoveWindow(g_splitter, g_splitterX, top, SPLITTER_WIDTH, paneHeight, TRUE);
    MoveWindow(compiledEditor->GetHWND(), g_splitterX + SPLITTER_WIDTH, top,
        max(0, width - g_splitterX - SPLITTER_WIDTH), paneHeight, TRUE);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;
    LoadSettingsFromFile();

    editor = new GutteredTextEditor();
    compiledEditor = new GutteredTextEditor();
    g_menuBar.LoadFromResource(hInstance, MAKEINTRESOURCEW(IDC_MIDE));

    g_debuggerClient = new MideDebuggerClient();

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

#ifdef STARTUPSPEED
    LARGE_INTEGER qpcEnd;
    QueryPerformanceCounter(&qpcEnd);

    double startupMs = (double)(qpcEnd.QuadPart - qpcStart.QuadPart) * 1000.0 / (double)qpcFreq.QuadPart;

    std::wstring title = L"mIDE (Ready in " + std::to_wstring(startupMs).substr(0, 4) + L" ms)";
    SetWindowTextW(hWnd, title.c_str());
#endif

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT lr = 0;
    if (DwmDefWindowProc(hWnd, message, wParam, lParam, &lr))
    {
        return lr;
    }

    if (message == WM_NCHITTEST && lr == 0)
    {
        lr = HitTestNCA(hWnd, wParam, lParam);
        return lr;
    }

    switch (message)
    {
    case WM_GETMINMAXINFO: {
        LPMINMAXINFO mmi = reinterpret_cast<LPMINMAXINFO>(lParam);
        mmi->ptMinTrackSize.x = 640;
        mmi->ptMinTrackSize.y = 480;
        return 0;
    }
    case WM_NCACTIVATE:
    case WM_NCLBUTTONDOWN: {
        RECT rcTitleBar;
        GetClientRect(hWnd, &rcTitleBar);
        rcTitleBar.bottom = TOPEXTENDWIDTH;
        g_isWindowActive = (wParam != FALSE);

        InvalidateRect(hWnd, &rcTitleBar, FALSE);
        UpdateWindow(hWnd);
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    case WM_NCCALCSIZE:
    {
        if (wParam == TRUE) {
            NCCALCSIZE_PARAMS* pncsp = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
            pncsp->rgrc[0].top += 0;
            pncsp->rgrc[0].right -= 8;
            pncsp->rgrc[0].left += 8;
            pncsp->rgrc[0].bottom -= 8;
            return 0;
        }
        else {
            RECT* prc = reinterpret_cast<RECT*>(lParam);
            prc->top += 0;
            prc->right -= 8;
            prc->left += 8;
            prc->bottom -= 8;
            return 0;
        }
    }
    case WM_CREATE:
    {
        ApplyDarkModeToWindow(hWnd);
        MARGINS m = {
            0,
            0,
            TOPEXTENDWIDTH,
            0
        };
        DwmExtendFrameIntoClientArea(hWnd, &m);
        if (!editor->Create(hInst, hWnd, 0, 0, 800, 600) ||
            !compiledEditor->Create(hInst, hWnd, 0, 0, 800, 600))
        {
            MessageBox(hWnd, L"Failed to create editor panes", L"Error", MB_OK | MB_ICONERROR);
            return -1;
        }
        editor->SetLintEnabled(g_settings.linterEnabled);
        compiledEditor->SetLintEnabled(false);
        compiledEditor->SetReadOnly(true);
        compiledEditor->SetText(L"Compiled mlog will appear here.");
        g_splitter = CreateWindowExW(
            0, L"mIDESplitter", nullptr, WS_CHILD | WS_VISIBLE,
            0, 0, SPLITTER_WIDTH, 0, hWnd, nullptr, hInst, nullptr);
    }
    break;
    case WM_SYSCOLORCHANGE:
    case WM_SETTINGCHANGE:
    {
        if (lParam && (wcscmp((LPWSTR)lParam, L"ImmersiveColorSet") == 0 || wcscmp((LPWSTR)lParam, L"WindowsThemeElement") == 0)) {
            if (g_darkModeSupported)
                g_darkModeEnabled = IsDarkMode() && !IsHighContrast();
            UpdateThemePalette();
            SetClassLongPtrW(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR)g_ThemeBrushes[CLR_WINDOW_BG]);
            ApplyDarkModeToWindow(hWnd);
            editor->RefreshTheme();
            InvalidateRect(hWnd, NULL, TRUE);
            SendMessageW(hWnd, WM_THEMECHANGED, 0, 0);
        }
    }
    break;
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDM_CMPCMIX:
        {
            DialogBox(hInst, MAKEINTRESOURCE(IDD_CMIX), hWnd, Trollge);
            break;
        }
        case IDM_COMPILE:
        {
            std::wstring sourceCode = editor->GetText();
            compiledEditor->SetText(L"Compiling and optimizing in background...\r\n");

                const bool optimizerEnabled = g_settings.optimizerEnabled;
                std::thread([hWnd, sourceCode, optimizerEnabled]() {
                    CompileResult result = CompileCToMlog(sourceCode, optimizerEnabled);
                auto* pResult = new CompileResult(result);
                PostMessageW(hWnd, WM_APP_COMPILE_DONE, reinterpret_cast<WPARAM>(pResult), 0);
                }).detach();
            break;
        }
        case IDM_LOAD_SAMPLE:
        {
            const std::wstring sample = LoadEmbeddedCompilerSample(hInst);
            if (sample.empty())
            {
                MessageBoxW(hWnd, L"Could not load the embedded compiler sample.",
                    L"Load Sample", MB_OK | MB_ICONERROR);
                break;
            }

            editor->SetText(sample);
            if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
            {
                compiledEditor->SetText(L"Sample loaded. Compiling and optimizing in background...\r\n");
                PostMessageW(hWnd, WM_COMMAND, MAKEWPARAM(IDM_COMPILE, 0), 0);
            }
            else
            {
                compiledEditor->SetText(L"Sample loaded. Press Compile to generate mlog.\r\n");
            }
            break;
        }
        case IDM_DEBUG:
        {
            compiledEditor->SetText(L"Connecting to Mindustry daemon in background...\r\n");

            const std::string host = ws2s(g_settings.host);
            const std::string port = std::to_string(g_settings.port);
            const DWORD timeoutMs = static_cast<DWORD>(max(1, g_settings.timeoutMs));

            std::thread([hWnd, host, port, timeoutMs]() {
                MideDebuggerClient client;

                if (client.connectToServer(host.c_str(), port.c_str(), timeoutMs)) {
                    std::string output = "CONNECTED!\r\n======================\r\n";
                    output += "PROCESSORS:\r\n" + client.sendCommand("LIST_PROCESSORS") + "\r\n";

                    client.sendCommand("SELECT_PROCESSOR 0");
                    output += "\r\nPROCESSOR [0] RAW CODE:\r\n";
                    output += client.sendCommand("GET_CODE") + "\r\n";

                    client.disconnect();

                    auto* pResult = new std::wstring(s2ws(output));
                    PostMessageW(hWnd, WM_APP_DEBUG_DONE, reinterpret_cast<WPARAM>(pResult), 0);
                }
                else {
                    auto* pResult = new std::wstring(
                        std::wstring(L"[Connection Error]\r\nMindustry is not running or port ") +
                        s2ws(port) + L" is blocked.");
                    PostMessageW(hWnd, WM_APP_DEBUG_DONE, reinterpret_cast<WPARAM>(pResult), 0);
                }
                }).detach();
            break;
        }
        case IDM_SETTINGS:
        {
            DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_SETTINGS), hWnd, (DLGPROC)SettingsDlgProc, 0);
            break;
        }
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        PaintCustomCaption(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_SIZE:
    {
        if (wParam == SIZE_MINIMIZED) return 0;
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (g_splitterX == 0)
            g_splitterX = (width - SPLITTER_WIDTH) / 2;
        ResizeEditorPanes(hWnd, width, height);
    }
    break;
    case WM_MOUSEMOVE: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        if (g_menuBar.OnMouseMove(hWnd, pt)) {
            return 0;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    case WM_MOUSELEAVE: {
        g_menuBar.OnMouseLeave(hWnd);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
        if (g_menuBar.OnLButtonDown(hWnd, pt)) {
            return 0;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    case WM_SYSCOMMAND: {
        if (g_menuBar.OnSysCommand(hWnd, wParam, lParam)) {
            return 0;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    case WM_APP_COMPILE_DONE:
    {
        auto* pResult = reinterpret_cast<CompileResult*>(wParam);
        if (pResult->success)
            compiledEditor->SetText(pResult->output);
        else
            compiledEditor->SetText(L"Compilation error (line " +
                std::to_wstring(pResult->errorLine) + L"):\r\n" + pResult->error);

        delete pResult;
        return 0;
    }
    case WM_APP_DEBUG_DONE:
    {
        auto* pText = reinterpret_cast<std::wstring*>(wParam);
        compiledEditor->SetText(*pText);
        delete pText;
        return 0;
    }
    case WM_DESTROY:
        delete g_debuggerClient;
        g_debuggerClient = nullptr;
        delete compiledEditor;
        compiledEditor = nullptr;
        delete editor;
        editor = nullptr;
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK SettingsNetworkPageProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        SetDlgItemTextW(hDlg, IDC_HOST, g_settings.host.c_str());
        SetDlgItemInt(hDlg, IDC_PORT, g_settings.port, FALSE);
        SetDlgItemInt(hDlg, IDC_TIMEOUT, g_settings.timeoutMs, FALSE);
        SendDlgItemMessageW(hDlg, IDC_SPEED, CB_ADDSTRING, 0, (LPARAM)L"Normal");
        SendDlgItemMessageW(hDlg, IDC_SPEED, CB_ADDSTRING, 0, (LPARAM)L"Slow");
        SendDlgItemMessageW(hDlg, IDC_SPEED, CB_ADDSTRING, 0, (LPARAM)L"Very slow");
        SendDlgItemMessageW(hDlg, IDC_SPEED, CB_SETCURSEL, g_settings.speedIndex, 0);
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_TEST_CONNECT)
        {
            wchar_t hostBuf[256] = {};
            wchar_t portBuf[32] = {};
            GetDlgItemTextW(hDlg, IDC_HOST, hostBuf, _countof(hostBuf));
            GetDlgItemTextW(hDlg, IDC_PORT, portBuf, _countof(portBuf));
            EnableWindow(GetDlgItem(hDlg, IDC_TEST_CONNECT), FALSE);

            std::wstring hostStr = hostBuf;
            int sizeNeeded = WideCharToMultiByte(
                CP_UTF8, 0, hostStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string hostUtf8(sizeNeeded, '\0');
            WideCharToMultiByte(
                CP_UTF8, 0, hostStr.c_str(), -1, hostUtf8.data(), sizeNeeded, nullptr, nullptr);
            std::string portStr = std::to_string(_wtoi(portBuf));
            wchar_t timeoutBuf[32] = {};
            GetDlgItemTextW(hDlg, IDC_TIMEOUT, timeoutBuf, _countof(timeoutBuf));
            DWORD timeoutMs = static_cast<DWORD>(max(1, _wtoi(timeoutBuf)));

            std::thread([hDlg, hostUtf8, portStr, timeoutMs]() {
                MideDebuggerClient client;
                bool ok = client.connectToServer(hostUtf8.c_str(), portStr.c_str(), timeoutMs);
                PostMessageW(hDlg, WM_TESTCONNECT_RESULT, ok ? 1 : 0, 0);
            }).detach();
            return TRUE;
        }
        break;

    case WM_TESTCONNECT_RESULT:
        EnableWindow(GetDlgItem(hDlg, IDC_TEST_CONNECT), TRUE);
        MessageBoxW(
            hDlg,
            wParam ? L"Connection OK" : L"Connection failed",
            L"Test connection",
            wParam ? MB_OK : MB_OK | MB_ICONERROR);
        return TRUE;
    }

    return FALSE;
}

INT_PTR CALLBACK SettingsCompilerPageProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_INITDIALOG)
    {
        CheckDlgButton(hDlg, IDC_COMPILER_OPTIMIZE,
            g_settings.optimizerEnabled ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_LINTER_ENABLE,
            g_settings.linterEnabled ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_LINTER_WARNINGS,
            g_settings.linterWarnings ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;
    }

    return FALSE;
}

INT_PTR CALLBACK SettingsCmixPageProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        SendDlgItemMessageW(hDlg, IDC_CMIX_PROFILE, CB_ADDSTRING, 0, (LPARAM)L"Balanced");
        SendDlgItemMessageW(hDlg, IDC_CMIX_PROFILE, CB_ADDSTRING, 0, (LPARAM)L"Maximum compression");
        SendDlgItemMessageW(hDlg, IDC_CMIX_PROFILE, CB_ADDSTRING, 0, (LPARAM)L"Fast");
        SendDlgItemMessageW(hDlg, IDC_CMIX_PROFILE, CB_SETCURSEL, g_settings.cmixProfile, 0);
        SetDlgItemInt(hDlg, IDC_CMIX_BLOCK_SIZE, g_settings.cmixBlockSize, FALSE);
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_CMIX_TEST)
        {
            DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_CMIX), hDlg, Trollge);
            return TRUE;
        }
        break;
    }

    return FALSE;
}

struct SettingsDialogState
{
    HWND tabs = nullptr;
    HWND pages[3] = {};
};

static void LayoutSettingsPages(HWND hDlg, SettingsDialogState* state)
{
    RECT pageRect = {};
    GetWindowRect(state->tabs, &pageRect);
    MapWindowPoints(nullptr, hDlg, reinterpret_cast<POINT*>(&pageRect), 2);
    TabCtrl_AdjustRect(state->tabs, FALSE, &pageRect);

    for (HWND page : state->pages)
        MoveWindow(page, pageRect.left, pageRect.top,
            pageRect.right - pageRect.left, pageRect.bottom - pageRect.top, TRUE);
}

static void SelectSettingsPage(SettingsDialogState* state, int selected)
{
    for (int index = 0; index < 3; ++index)
        ShowWindow(state->pages[index], index == selected ? SW_SHOW : SW_HIDE);
}

INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<SettingsDialogState*>(
        GetWindowLongPtrW(hDlg, DWLP_USER));

    switch (message)
    {
    case WM_INITDIALOG:
    {
        auto* newState = new SettingsDialogState();
        SetWindowLongPtrW(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(newState));
        newState->tabs = GetDlgItem(hDlg, IDC_SETTINGSTABBER);

        TCITEMW item = {};
        item.mask = TCIF_TEXT;
        wchar_t labels[][32] = { L"Connection", L"Compiler / Linter", L"CMIX" };
        for (wchar_t* label : labels)
        {
            item.pszText = label;
            TabCtrl_InsertItem(newState->tabs, TabCtrl_GetItemCount(newState->tabs), &item);
        }

        newState->pages[0] = CreateDialogW(
            hInst, MAKEINTRESOURCEW(IDD_SETTINGS_NETWORK), hDlg,
            SettingsNetworkPageProc);
        newState->pages[1] = CreateDialogW(
            hInst, MAKEINTRESOURCEW(IDD_SETTINGS_COMPILER), hDlg,
            SettingsCompilerPageProc);
        newState->pages[2] = CreateDialogW(
            hInst, MAKEINTRESOURCEW(IDD_SETTINGS_CMIX), hDlg,
            SettingsCmixPageProc);

        if (!newState->pages[0] || !newState->pages[1] || !newState->pages[2])
        {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }

        LayoutSettingsPages(hDlg, newState);
        TabCtrl_SetCurSel(newState->tabs, 0);
        SelectSettingsPage(newState, 0);
        return TRUE;
    }

    case WM_NOTIFY:
    {
        auto* header = reinterpret_cast<NMHDR*>(lParam);
        if (state && header->idFrom == IDC_SETTINGSTABBER &&
            header->code == TCN_SELCHANGE)
        {
            SelectSettingsPage(state, TabCtrl_GetCurSel(state->tabs));
            return TRUE;
        }
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK && state)
        {
            wchar_t buffer[256] = {};
            GetDlgItemTextW(state->pages[0], IDC_HOST, buffer, _countof(buffer));
            g_settings.host = buffer;
            GetDlgItemTextW(state->pages[0], IDC_PORT, buffer, _countof(buffer));
            g_settings.port = _wtoi(buffer);
            GetDlgItemTextW(state->pages[0], IDC_TIMEOUT, buffer, _countof(buffer));
            g_settings.timeoutMs = _wtoi(buffer);
            g_settings.speedIndex = static_cast<int>(
                SendDlgItemMessageW(state->pages[0], IDC_SPEED, CB_GETCURSEL, 0, 0));
            g_settings.optimizerEnabled =
                IsDlgButtonChecked(state->pages[1], IDC_COMPILER_OPTIMIZE) == BST_CHECKED;
            g_settings.linterEnabled =
                IsDlgButtonChecked(state->pages[1], IDC_LINTER_ENABLE) == BST_CHECKED;
            g_settings.linterWarnings =
                IsDlgButtonChecked(state->pages[1], IDC_LINTER_WARNINGS) == BST_CHECKED;
            g_settings.cmixProfile = static_cast<int>(
                SendDlgItemMessageW(state->pages[2], IDC_CMIX_PROFILE, CB_GETCURSEL, 0, 0));
            g_settings.cmixBlockSize = GetDlgItemInt(
                state->pages[2], IDC_CMIX_BLOCK_SIZE, nullptr, FALSE);
            editor->SetLintEnabled(g_settings.linterEnabled);
            SaveSettingsToFile();
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_DESTROY:
        if (state)
        {
            for (HWND page : state->pages)
                if (page) DestroyWindow(page);
            delete state;
            SetWindowLongPtrW(hDlg, DWLP_USER, 0);
        }
        return TRUE;
    }

    return FALSE;
}

// (Legacy programmatic settings window left in file for now; resource dialog used instead.)
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
        UNREFERENCED_PARAMETER(lParam);
        switch (message)
        {
        case WM_INITDIALOG:
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
        }
        return (INT_PTR)FALSE;
}

INT_PTR CALLBACK Trollge(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
        UNREFERENCED_PARAMETER(lParam);
        switch (message)
        {
        case WM_INITDIALOG:
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
        }
        return (INT_PTR)FALSE;
}
