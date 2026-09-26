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
#include <string>

#pragma comment(lib, "ws2_32.lib")

#define MAX_LOADSTRING 100

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

    bool connectToServer(const char* ip, const char* port) {
        if (connectSocket != INVALID_SOCKET) return true;

        struct addrinfo hints = { 0 }, * result = NULL;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        if (getaddrinfo(ip, port, &hints, &result) != 0) return false;

        connectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

        DWORD timeoutMs = 2000;
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

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    Trollge(HWND, UINT, WPARAM, LPARAM);
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

    INITCOMMONCONTROLSEX commonControls = {};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
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

            std::thread([hWnd, sourceCode]() {
                CompileResult result = CompileCToMlog(sourceCode);
                auto* pResult = new CompileResult(result);
                PostMessageW(hWnd, WM_APP_COMPILE_DONE, reinterpret_cast<WPARAM>(pResult), 0);
                }).detach();
            break;
        }
        case IDM_DEBUG:
        {
            compiledEditor->SetText(L"Connecting to Mindustry daemon in background...\r\n");

            std::thread([hWnd]() {
                MideDebuggerClient client;

                if (client.connectToServer("127.0.0.1", "9999")) {
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
                    auto* pResult = new std::wstring(L"[Connection Error]\r\nMindustry is not running or port 9999 is blocked.");
                    PostMessageW(hWnd, WM_APP_DEBUG_DONE, reinterpret_cast<WPARAM>(pResult), 0);
                }
                }).detach();
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