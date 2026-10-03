#include <vector>
#include <string>
#include <set>
#include <map>
#include <thread>
#include <atomic>
#include <cwctype>
#include "MindustryCallTips.inl"

static constexpr wchar_t GUTTERED_TEXT_EDITOR_CLASS[] = L"GutteredTextEditor";
static constexpr UINT WM_GUTTERED_TEXT_EDITOR_REFRESH_GUTTER = WM_USER + 2;
constexpr UINT_PTR TIMER_LINT_ID = 1005;

static int g_errorLine = -1;
static bool s_contextMenuOpen = false;
static std::wstring g_errorMessage = L"";

#define IDM_EDIT_UNDO       2001
#define IDM_EDIT_REDO       2002
#define IDM_EDIT_CUT        2003
#define IDM_EDIT_COPY       2004
#define IDM_EDIT_PASTE      2005
#define IDM_EDIT_DELETE     2006
#define IDM_EDIT_SELECTALL  2007
#define IDM_EDIT_COMPILE    2008


static const std::set<std::wstring> g_TypeKeywords = {
    L"int", L"void", L"char", L"short", L"long", L"signed", L"unsigned",
    L"bool", L"_Bool", L"INT_PTR", L"double", L"float", L"const",
    L"static", L"extern", L"auto", L"register", L"inline", L"restrict",
    L"true", L"false"
};

static const std::set<std::wstring> g_DeclarationKeywords = {
    L"int", L"void", L"char", L"short", L"long", L"signed", L"unsigned",
    L"bool", L"_Bool", L"INT_PTR", L"double", L"float", L"const",
    L"static", L"extern", L"auto", L"register", L"inline", L"restrict"
};


static const std::set<std::wstring> g_ControlKeywords = {
    L"return", L"if", L"else", L"while", L"for", L"break", L"continue"
};


static const std::set<std::wstring> g_MindustryKeywords = {
    L"hyper", L"micro", L"logic"
};


static const std::set<std::wstring> g_BuiltinFunctions = {
    L"print", L"eightop"
};


static const std::vector<std::wstring> g_AllAutocompleteKeywords = {
    L"int", L"return", L"void", L"char", L"short", L"long", L"signed", L"unsigned",
    L"bool", L"_Bool", L"INT_PTR", L"double", L"float", L"const",
    L"static", L"extern", L"auto", L"register", L"inline", L"restrict",
    L"true", L"false",
    L"if", L"else", L"while", L"for", L"break", L"continue",
    L"hyper", L"micro", L"logic",
};

struct LintResult {
    uint64_t hash;
    bool success;
    int errorLine;
    std::wstring error;
};

class GutteredTextEditor
{
public:
    GutteredTextEditor() : hwnd_(nullptr) {}
    ~GutteredTextEditor() { if (hwnd_) DestroyWindow(hwnd_); }

    static ATOM RegisterWindowClass(HINSTANCE hInstance)
    {
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wcex.lpfnWndProc = WindowProcStatic;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = sizeof(GutteredTextEditor*);
        wcex.hInstance = hInstance;
        wcex.hIcon = nullptr;
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = GUTTERED_TEXT_EDITOR_CLASS;
        wcex.hIconSm = nullptr;
        return RegisterClassExW(&wcex);
    }

    bool Create(HINSTANCE hInstance, HWND parent, int x = CW_USEDEFAULT, int y = CW_USEDEFAULT,
        int width = CW_USEDEFAULT, int height = CW_USEDEFAULT,
        DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        DWORD exStyle = 0)
    {
        hwnd_ = CreateWindowExW(
            exStyle,
            GUTTERED_TEXT_EDITOR_CLASS,
            L"",
            style,
            x, y, width, height,
            parent,
            nullptr,
            hInstance,
            this
        );
        return hwnd_ != nullptr;
    }

    HWND GetHWND() const { return hwnd_; }
    HWND GetGutterHWND() const { return hwndGutter_; }
    HWND GetEditHWND() const { return hwndEdit_; }
    HFONT GetFont() const { return hEditorFont; }
    const std::set<std::wstring>& GetKnownVariables() const { return knownVariables_; }
    const std::set<std::wstring>& GetKnownFunctions() const { return knownFunctions_; }

    std::wstring GetText() const
    {
        if (!hwndEdit_)
            return {};

        const int length = GetWindowTextLengthW(hwndEdit_);
        std::wstring text(length, L'\0');
        if (length > 0)
            GetWindowTextW(hwndEdit_, text.data(), length + 1);
        return text;
    }

    void SetText(const std::wstring& text)
    {
        if (hwndEdit_)
        {
            SetWindowTextW(hwndEdit_, text.c_str());
            if (lintEnabled_ && hwnd_)
            {
                SetTimer(hwnd_, TIMER_LINT_ID, 1, nullptr);
            }
        }
    }

    void SetReadOnly(bool readOnly)
    {
        if (hwndEdit_)
            SendMessageW(hwndEdit_, EM_SETREADONLY, readOnly ? TRUE : FALSE, 0);
    }

    void SetLintEnabled(bool enabled)
    {
        lintEnabled_ = enabled;
        if (!enabled)
        {
            if (hwnd_)
                KillTimer(hwnd_, TIMER_LINT_ID);

            latestLintHash_ = 0;
            lastCompletedLintHash_ = 0;
            g_errorLine = -1;
            g_errorMessage.clear();

            if (hwndPopup_)
                ShowWindow(hwndPopup_, SW_HIDE);
            if (hwndEdit_)
                HighlightSyntax();
            if (hwndGutter_)
                InvalidateRect(hwndGutter_, nullptr, FALSE);
        }
        else if (hwnd_)
        {
            SetTimer(hwnd_, TIMER_LINT_ID, 1, nullptr);
        }
    }

    void RefreshTheme()
    {
        if (hwndEdit_)
            ApplyThemeToRichEdit(hwndEdit_);
        if (hwnd_)
            InvalidateRect(hwnd_, nullptr, TRUE);
    }

    void HighlightSyntax()
    {
        if (!hwndEdit_) return;

        SendMessageW(hwndEdit_, WM_SETREDRAW, FALSE, 0);

        CHARRANGE origSel = { 0, 0 };
        SendMessageW(hwndEdit_, EM_EXGETSEL, 0, (LPARAM)&origSel);
        POINT scrollPos = { 0, 0 };
        SendMessageW(hwndEdit_, EM_GETSCROLLPOS, 0, (LPARAM)&scrollPos);

        LRESULT oldMask = SendMessageW(hwndEdit_, EM_GETEVENTMASK, 0, 0);
        SendMessageW(hwndEdit_, EM_SETEVENTMASK, 0, oldMask & ~ENM_CHANGE);

        GETTEXTLENGTHEX gtl = { GTL_NUMCHARS | GTL_PRECISE, CP_UTF8 };
        int charCount = (int)SendMessageW(hwndEdit_, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);

        if (charCount > 0)
        {
            std::vector<wchar_t> text(charCount + 1);
            TEXTRANGEW tr = { { 0, charCount }, text.data() };
            SendMessageW(hwndEdit_, EM_GETTEXTRANGE, 0, (LPARAM)&tr);

            auto SetColor = [&](int start, int end, COLORREF color) {
                CHARRANGE cr = { start, end };
                SendMessageW(hwndEdit_, EM_EXSETSEL, 0, (LPARAM)&cr);
                CHARFORMAT2W cf = { sizeof(cf) };
                cf.dwMask = CFM_COLOR;
                cf.crTextColor = color;
                SendMessageW(hwndEdit_, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
                };

            
            SetColor(0, charCount, RGB(220, 220, 220));

            
            knownVariables_.clear();
            knownFunctions_ = g_BuiltinFunctions; 

            for (const auto& tip : g_CallTipSignatures)
                knownFunctions_.insert(tip.first);
            
            int scanPos = 0;
            while (scanPos < charCount)
            {
                if (iswspace(text[scanPos])) { scanPos++; continue; }

                if (text[scanPos] == L'/' && scanPos + 1 < charCount && text[scanPos + 1] == L'/')
                {
                    while (scanPos < charCount && text[scanPos] != L'\r' && text[scanPos] != L'\n') scanPos++;
                    continue;
                }

                if (iswalpha(text[scanPos]) || text[scanPos] == L'_')
                {
                    int start = scanPos;
                    while (scanPos < charCount && (iswalnum(text[scanPos]) || text[scanPos] == L'_')) scanPos++;
                    std::wstring word(&text[start], scanPos - start);

                    
                    if (g_DeclarationKeywords.count(word))
                    {
                        while (scanPos < charCount)
                        {
                            while (scanPos < charCount && iswspace(text[scanPos])) scanPos++;
                            if (scanPos >= charCount ||
                                (!iswalpha(text[scanPos]) && text[scanPos] != L'_'))
                                break;

                            int idStart = scanPos;
                            while (scanPos < charCount &&
                                (iswalnum(text[scanPos]) || text[scanPos] == L'_'))
                                scanPos++;
                            std::wstring idName(&text[idStart], scanPos - idStart);

                            if (g_DeclarationKeywords.count(idName))
                                continue;

                            int peek = scanPos;
                            while (peek < charCount && iswspace(text[peek])) peek++;
                            if (peek < charCount && text[peek] == L'(')
                                knownFunctions_.insert(idName); 
                            else
                                knownVariables_.insert(idName); 
                            break;
                        }
                    }
                    continue;
                }
                scanPos++;
            }

            
            int pos = 0;
            while (pos < charCount)
            {
                if (iswspace(text[pos])) { pos++; continue; }
                
                if (text[pos] == L'/' && pos + 1 < charCount && text[pos + 1] == L'/')
                {
                    int start = pos;
                    while (pos < charCount && text[pos] != L'\r' && text[pos] != L'\n') pos++;
                    SetColor(start, pos, RGB(106, 153, 85));
                    continue;
                }

                
                if (text[pos] == L'#')
                {
                    int start = pos;
                    while (pos < charCount && (iswalnum(text[pos]) || text[pos] == L'_' || text[pos] == L'#')) pos++;
                    SetColor(start, pos, RGB(197, 134, 192));
                    continue;
                }

                if (text[pos] == L'"')
                {
                    int start = pos++;
                    while (pos < charCount && text[pos] != L'"' && text[pos] != L'\r' && text[pos] != L'\n') {
                        if (text[pos] == L'\\' && pos + 1 < charCount) pos++;
                        pos++;
                    }
                    if (pos < charCount && text[pos] == L'"') pos++;
                    SetColor(start, pos, RGB(214, 157, 133));
                    continue;
                }

                if (iswdigit(text[pos]))
                {
                    int start = pos;
                    while (pos < charCount && iswdigit(text[pos])) pos++;
                    SetColor(start, pos, RGB(181, 206, 168));
                    continue;
                }

                if (iswalpha(text[pos]) || text[pos] == L'_')
                {
                    int start = pos;
                    while (pos < charCount && (iswalnum(text[pos]) || text[pos] == L'_')) pos++;
                    std::wstring word(&text[start], pos - start);

                    if (g_TypeKeywords.count(word) || g_ControlKeywords.count(word))
                    {
                        SetColor(start, pos, RGB(86, 156, 214)); 
                    }
                    else if (g_MindustryKeywords.count(word))
                    {
                        SetColor(start, pos, RGB(78, 201, 176)); 
                    }
                    else if (knownFunctions_.count(word))
                    {
                        SetColor(start, pos, RGB(220, 220, 170)); 
                    }
                    else if (knownVariables_.count(word))
                    {
                        SetColor(start, pos, RGB(156, 220, 254)); 
                    }
                    continue;
                }

                if (text[pos] == L'@')
                {
                    int start = pos;
                    while (pos < charCount && (iswalnum(text[pos]) || text[pos] == L'_' || text[pos] == L'@' || text[pos] == L'-'))
                        pos++;

                    // Mindustry Teal / Cyan Color:
                    SetColor(start, pos, RGB(78, 201, 176));
                    continue;
                }

                pos++;
            }
        }

        SendMessageW(hwndEdit_, EM_EXSETSEL, 0, (LPARAM)&origSel);
        SendMessageW(hwndEdit_, EM_SETSCROLLPOS, 0, (LPARAM)&scrollPos);
        SendMessageW(hwndEdit_, EM_SETEVENTMASK, 0, oldMask);
        SendMessageW(hwndEdit_, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(hwndEdit_, nullptr, FALSE);
    }

private:
    HWND hwnd_;
    HWND hwndGutter_;
    HWND hwndEdit_;
    HWND hwndPopup_ = nullptr;
    int wordStartIndex_ = 0;
    HWND hwndTooltip_ = nullptr;
    TOOLINFOW toolInfo_ = {};
    HFONT hEditorFont = nullptr;
    bool lintEnabled_ = true;
    std::set<std::wstring> knownVariables_;
    std::set<std::wstring> knownFunctions_;

    std::atomic<uint64_t> latestLintHash_{ 0 };
    uint64_t lastCompletedLintHash_ = 0;

    uint64_t HashString64(const std::wstring& str)
    {
        uint64_t hash = 14695981039346656037ULL;
        for (wchar_t c : str)
        {
            hash ^= static_cast<uint64_t>(c);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    void CreateCallTipTooltip(HINSTANCE hInst)
    {
        hwndTooltip_ = CreateWindowExW(
            WS_EX_TOPMOST,
            TOOLTIPS_CLASS,
            nullptr,
            WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            hwndEdit_, nullptr, hInst, nullptr
        );

        toolInfo_.cbSize = sizeof(TOOLINFOW);
        toolInfo_.uFlags = TTF_TRACK | TTF_ABSOLUTE;
        toolInfo_.hwnd = hwndEdit_;
        toolInfo_.uId = 1;
        toolInfo_.lpszText = nullptr;

        SendMessageW(hwndTooltip_, TTM_ADDTOOL, 0, (LPARAM)&toolInfo_);
    }

    void ShowCallTip(const std::wstring& tipText)
    {
        if (!hwndTooltip_ || !hwndEdit_) return;

        toolInfo_.lpszText = const_cast<LPWSTR>(tipText.c_str());
        SendMessageW(hwndTooltip_, TTM_UPDATETIPTEXTW, 0, (LPARAM)&toolInfo_);

        CHARRANGE cr = {};
        SendMessageW(hwndEdit_, EM_EXGETSEL, 0, (LPARAM)&cr);
        POINTL pt = {};
        SendMessageW(hwndEdit_, EM_POSFROMCHAR, (WPARAM)&pt, cr.cpMax);

        POINT screenPt = { (LONG)pt.x, (LONG)pt.y };
        ClientToScreen(hwndEdit_, &screenPt);

        int tipY = (pt.y < 30) ? (screenPt.y + 24) : (screenPt.y - 25);

        SendMessageW(hwndTooltip_, TTM_TRACKPOSITION, 0, MAKELPARAM(screenPt.x, tipY));
        SendMessageW(hwndTooltip_, TTM_TRACKACTIVATE, TRUE, (LPARAM)&toolInfo_);
    }

    void HideCallTip()
    {
        if (hwndTooltip_)
            SendMessageW(hwndTooltip_, TTM_TRACKACTIVATE, FALSE, (LPARAM)&toolInfo_);
    }

    void PositionAutocompletePopup()
    {
        if (!hwndEdit_ || !hwndPopup_) return;

        CHARRANGE cr = {};
        SendMessageW(hwndEdit_, EM_EXGETSEL, 0, (LPARAM)&cr);
        int caretPos = max(0, cr.cpMax);

        POINTL pt = {};
        SendMessageW(hwndEdit_, EM_POSFROMCHAR, (WPARAM)&pt, caretPos);

        POINT screenPt = { (LONG)pt.x, (LONG)pt.y };
        ClientToScreen(hwndEdit_, &screenPt);

        SetWindowPos(hwndPopup_, HWND_TOPMOST,
            screenPt.x, screenPt.y + 22,
            180, 120,
            SWP_SHOWWINDOW | SWP_NOACTIVATE);
    }

    void UpdateAutocomplete()
    {
        if (!hwndEdit_ || !hwndPopup_ || !lintEnabled_) return;

        CHARRANGE cr = {};
        SendMessageW(hwndEdit_, EM_EXGETSEL, 0, (LPARAM)&cr);
        int caret = cr.cpMax;
        if (caret <= 0) { ShowWindow(hwndPopup_, SW_HIDE); return; }

        int totalLen = GetWindowTextLengthW(hwndEdit_);
        if (totalLen == 0) { ShowWindow(hwndPopup_, SW_HIDE); return; }

        std::vector<wchar_t> fullText(totalLen + 1, 0);
        TEXTRANGEW tr = { { 0, totalLen }, fullText.data() };
        SendMessageW(hwndEdit_, EM_GETTEXTRANGE, 0, (LPARAM)&tr);

        if (caret > totalLen) caret = totalLen;

        
        int start = caret;
        while (start > 0 && (iswalnum(fullText[start - 1]) || fullText[start - 1] == L'_')) {
            start--;
        }

        wordStartIndex_ = start;
        int wordLen = caret - start;

        if (wordLen < 2) {
            ShowWindow(hwndPopup_, SW_HIDE);
            return;
        }

        std::wstring prefix(&fullText[start], wordLen);

        SendMessageW(hwndPopup_, LB_RESETCONTENT, 0, 0);

		std::set<std::wstring> uniqueSuggestions;

        auto AddIfMatch = [&](const std::wstring& candidate) {
            if (candidate.size() > prefix.size()) {
                if (_wcsnicmp(candidate.c_str(), prefix.c_str(), prefix.size()) == 0) {
                    uniqueSuggestions.insert(candidate);
                }
            }
            };

        
        for (const auto& k : g_AllAutocompleteKeywords) AddIfMatch(k);
        for (const auto& f : knownFunctions_) AddIfMatch(f);
        for (const auto& v : knownVariables_) AddIfMatch(v);
        for (const auto& tip : g_CallTipSignatures) AddIfMatch(tip.first);

        if (!uniqueSuggestions.empty()) {
            for (const auto& word : uniqueSuggestions) {
                SendMessageW(hwndPopup_, LB_ADDSTRING, 0, (LPARAM)word.c_str());
            }
            SendMessageW(hwndPopup_, LB_SETCURSEL, 0, 0);
            PositionAutocompletePopup();
        }
        else {
            ShowWindow(hwndPopup_, SW_HIDE);
        }
    }

    static LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_NCCREATE)
        {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            if (!cs || !cs->lpCreateParams)
            {
                return DefWindowProcW(hwnd, msg, wParam, lParam);
            }
            auto* self = reinterpret_cast<GutteredTextEditor*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->hwnd_ = hwnd;
            return TRUE;
        }

        auto* self = reinterpret_cast<GutteredTextEditor*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self)
            return self->WindowProc(hwnd, msg, wParam, lParam);

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    static LRESULT CALLBACK GutterSubclassProc(
        HWND hWnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR uIdSubclass,
        DWORD_PTR dwRefData) {
        switch (uMsg) {
        case WM_PAINT: {
            auto* self = reinterpret_cast<GutteredTextEditor*>(dwRefData);
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc;
            GetClientRect(hWnd, &rc);
            FillRect(hdc, &rc, g_ThemeBrushes[CLR_GUTTER_BG]);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, g_ThemeColors[CLR_GUTTER_TEXT]);
            if (self->hEditorFont)
            {
                HFONT oldFont = (HFONT)SelectObject(hdc, self->hEditorFont);
                int firstVisibleVisualLine = (int)SendMessageW(self->hwndEdit_, EM_GETFIRSTVISIBLELINE, 0, 0);
                int totalVisualLines = (int)SendMessageW(self->hwndEdit_, EM_GETLINECOUNT, 0, 0);

                int firstChar = (int)SendMessageW(self->hwndEdit_, EM_LINEINDEX, firstVisibleVisualLine, 0);
                int logicalLine = (int)SendMessageW(self->hwndEdit_, EM_EXLINEFROMCHAR, 0, firstChar) + 1;

                for (int visualLine = firstVisibleVisualLine; visualLine < totalVisualLines; ++visualLine)
                {
                    int charIndex = (int)SendMessageW(self->hwndEdit_, EM_LINEINDEX, visualLine, 0);
                    POINT pt = {};
                    SendMessageW(self->hwndEdit_, EM_POSFROMCHAR, (WPARAM)&pt, charIndex);
                    if (pt.y > rc.bottom)
                        break;

                    bool isLogicalStart = false;
                    if (charIndex == 0)
                    {
                        isLogicalStart = true;
                    }
                    else
                    {
                        TEXTRANGEW tr = {};
                        tr.chrg.cpMin = charIndex - 1;
                        tr.chrg.cpMax = charIndex;
                        wchar_t prevChar[2] = { 0, 0 };
                        tr.lpstrText = prevChar;
                        SendMessageW(self->hwndEdit_, EM_GETTEXTRANGE, 0, (LPARAM)&tr);

                        if (prevChar[0] == L'\r')
                        {
                            isLogicalStart = true;
                        }
                    }

                    if (isLogicalStart)
                    {
                        wchar_t numStr[16];
                        wsprintfW(numStr, L"%d", logicalLine++);
                        RECT textRc = { 0, pt.y, rc.right - 5, pt.y + 20 };
                        if (logicalLine == g_errorLine)
                        {
                            SetTextColor(hdc, RGB(255, 60, 60));
                        }
                        else
                        {
                            SetTextColor(hdc, g_ThemeColors[CLR_GUTTER_TEXT]);
                        }
                        DrawTextW(hdc, numStr, -1, &textRc, DT_RIGHT | DT_TOP | DT_SINGLELINE);
                    }
                }
                SelectObject(hdc, oldFont);
            }
            EndPaint(hWnd, &ps);
            return 0;
        }
        }
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    static LRESULT CALLBACK RichEditSubclassProc(
        HWND hWnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR uIdSubclass,
        DWORD_PTR dwRefData)
    {
        auto* self = reinterpret_cast<GutteredTextEditor*>(dwRefData);
        if (!self) return DefSubclassProc(hWnd, uMsg, wParam, lParam);

        static int s_dragStartLine = -1;

        switch (uMsg)
        {
        case WM_CHAR:
        {
            wchar_t ch = static_cast<wchar_t>(wParam);

            if (ch == L'(')
            {
                CHARRANGE cr = {};
                SendMessageW(hWnd, EM_EXGETSEL, 0, (LPARAM)&cr);
                int pos = cr.cpMax;

                int startPos = max(0, pos - 64);
                std::vector<wchar_t> textBuf(pos - startPos + 1, 0);
                TEXTRANGEW tr = { { startPos, pos }, textBuf.data() };
                SendMessageW(hWnd, EM_GETTEXTRANGE, 0, (LPARAM)&tr);

                int end = (int)wcslen(textBuf.data());
                while (end > 0 && iswspace(textBuf[end - 1])) end--;
                int start = end;
                while (start > 0 && (iswalnum(textBuf[start - 1]) || textBuf[start - 1] == L'_')) start--;

                if (end > start)
                {
                    std::wstring funcName(&textBuf[start], end - start);

                    auto it = g_CallTipSignatures.find(funcName);
                    if (it != g_CallTipSignatures.end())
                    {
                        self->ShowCallTip(it->second);
                    }
                    else if (self->knownFunctions_.count(funcName))
                    {
                        self->ShowCallTip(funcName + L"(...)");
                    }
                }
            }
            else if (ch == L')' || ch == L';')
            {
                self->HideCallTip();
            }
            break;
        }
        case WM_CONTEXTMENU:
        {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            if (pt.x == -1 && pt.y == -1)
            {
                GetCaretPos(&pt);
                ClientToScreen(hWnd, &pt);
            }

            CHARRANGE cr = { 0, 0 };
            SendMessageW(hWnd, EM_EXGETSEL, 0, (LPARAM)&cr);
            bool hasSelection = (cr.cpMin != cr.cpMax);
            bool canUndo = SendMessageW(hWnd, EM_CANUNDO, 0, 0) != 0;
            bool canRedo = SendMessageW(hWnd, EM_CANREDO, 0, 0) != 0;
            bool canPaste = IsClipboardFormatAvailable(CF_UNICODETEXT) != 0;

            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, canUndo ? MF_STRING : (MF_STRING | MF_GRAYED), IDM_EDIT_UNDO, L"&Undo\tCtrl+Z");
            AppendMenuW(hMenu, canRedo ? MF_STRING : (MF_STRING | MF_GRAYED), IDM_EDIT_REDO, L"&Redo\tCtrl+Y");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, hasSelection ? MF_STRING : (MF_STRING | MF_GRAYED), IDM_EDIT_CUT, L"Cu&t\tCtrl+X");
            AppendMenuW(hMenu, hasSelection ? MF_STRING : (MF_STRING | MF_GRAYED), IDM_EDIT_COPY, L"&Copy\tCtrl+C");
            AppendMenuW(hMenu, canPaste ? MF_STRING : (MF_STRING | MF_GRAYED), IDM_EDIT_PASTE, L"&Paste\tCtrl+V");
            AppendMenuW(hMenu, hasSelection ? MF_STRING : (MF_STRING | MF_GRAYED), IDM_EDIT_DELETE, L"&Delete");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, IDM_EDIT_SELECTALL, L"Select &All\tCtrl+A");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, IDM_EDIT_COMPILE, L"&Compile to MLog\tCtrl+B");

            s_contextMenuOpen = true;

            int cmd = TrackPopupMenuEx(
                hMenu,
                TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON,
                pt.x, pt.y,
                hWnd,
                nullptr
            );

            s_contextMenuOpen = false;
            DestroyMenu(hMenu);

            switch (cmd)
            {
            case IDM_EDIT_UNDO:      SendMessageW(hWnd, EM_UNDO, 0, 0); break;
            case IDM_EDIT_REDO:      SendMessageW(hWnd, EM_REDO, 0, 0); break;
            case IDM_EDIT_CUT:       SendMessageW(hWnd, WM_CUT, 0, 0); break;
            case IDM_EDIT_COPY:      SendMessageW(hWnd, WM_COPY, 0, 0); break;
            case IDM_EDIT_PASTE:     SendMessageW(hWnd, WM_PASTE, 0, 0); break;
            case IDM_EDIT_DELETE:    SendMessageW(hWnd, WM_CLEAR, 0, 0); break;
            case IDM_EDIT_SELECTALL: {
                CHARRANGE all = { 0, -1 };
                SendMessageW(hWnd, EM_EXSETSEL, 0, (LPARAM)&all);
                break;
            }
            case IDM_EDIT_COMPILE: {
                PostMessageW(GetAncestor(hWnd, GA_ROOT), WM_COMMAND, MAKEWPARAM(IDM_COMPILE, 0), 0);
                break;
            }
            }

            return 0;
        }
        case WM_SETCURSOR:
        {
            if (s_contextMenuOpen) {
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                return TRUE;
            }
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);

            if (pt.x >= 0 && pt.x < IDE_GUTTER_WIDTH)
            {
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                return TRUE;
            }
            break;
        }

        case WM_LBUTTONDOWN:
        {
            short x = (short)LOWORD(lParam);
            short y = (short)HIWORD(lParam);

            if (x >= 0 && x < IDE_GUTTER_WIDTH)
            {
                POINTL ptl = { 50, y };
                int charIdx = (int)SendMessageW(hWnd, EM_CHARFROMPOS, 0, (LPARAM)&ptl);
                int startLine = (int)SendMessageW(hWnd, EM_EXLINEFROMCHAR, 0, charIdx);

                int lineStartChar = (int)SendMessageW(hWnd, EM_LINEINDEX, startLine, 0);
                int lineLength = (int)SendMessageW(hWnd, EM_LINELENGTH, charIdx, 0);

                CHARRANGE cr = { lineStartChar, lineStartChar + lineLength + 1 };
                SendMessageW(hWnd, EM_EXSETSEL, 0, (LPARAM)&cr);

                s_dragStartLine = startLine;
                SetCapture(hWnd);
                return 0;
            }
            break;
        }

        case WM_MOUSEMOVE:
        {
            if (GetCapture() == hWnd && s_dragStartLine != -1)
            {
                short y = (short)HIWORD(lParam);

                POINTL ptl = { 50, y };
                int charIdx = (int)SendMessageW(hWnd, EM_CHARFROMPOS, 0, (LPARAM)&ptl);
                int currentLine = (int)SendMessageW(hWnd, EM_EXLINEFROMCHAR, 0, charIdx);

                int topRow = min(s_dragStartLine, currentLine);
                int bottomRow = max(s_dragStartLine, currentLine);

                int startIdx = (int)SendMessageW(hWnd, EM_LINEINDEX, topRow, 0);
                int bottomLineStart = (int)SendMessageW(hWnd, EM_LINEINDEX, bottomRow, 0);
                int bottomLineLength = (int)SendMessageW(hWnd, EM_LINELENGTH, bottomLineStart, 0);
                int endIdx = bottomLineStart + bottomLineLength + 1;

                CHARRANGE cr;
                if (currentLine >= s_dragStartLine)
                {
                    cr.cpMin = startIdx;
                    cr.cpMax = endIdx;
                }
                else
                {
                    cr.cpMin = endIdx;
                    cr.cpMax = startIdx;
                }

                SendMessageW(hWnd, EM_EXSETSEL, 0, (LPARAM)&cr);
                return 0;
            }
            break;
        }

        case WM_LBUTTONUP:
        {
            if (GetCapture() == hWnd && s_dragStartLine != -1)
            {
                ReleaseCapture();
                s_dragStartLine = -1;
                return 0;
            }
            break;
        }
        case WM_KEYDOWN:
        {
            if (self->hwndPopup_ && IsWindowVisible(self->hwndPopup_))
            {
                if (wParam == VK_DOWN)
                {
                    int sel = (int)SendMessageW(self->hwndPopup_, LB_GETCURSEL, 0, 0);
                    int count = (int)SendMessageW(self->hwndPopup_, LB_GETCOUNT, 0, 0);
                    SendMessageW(self->hwndPopup_, LB_SETCURSEL, min(count - 1, sel + 1), 0);
                    return 0;
                }
                if (wParam == VK_UP)
                {
                    int sel = (int)SendMessageW(self->hwndPopup_, LB_GETCURSEL, 0, 0);
                    SendMessageW(self->hwndPopup_, LB_SETCURSEL, max(0, sel - 1), 0);
                    return 0;
                }
                if (wParam == VK_TAB || wParam == VK_RETURN)
                {
                    int sel = (int)SendMessageW(self->hwndPopup_, LB_GETCURSEL, 0, 0);
                    if (sel != LB_ERR)
                    {
                        int len = (int)SendMessageW(self->hwndPopup_, LB_GETTEXTLEN, sel, 0);
                        std::vector<wchar_t> chosen(len + 1, 0);
                        SendMessageW(self->hwndPopup_, LB_GETTEXT, sel, (LPARAM)chosen.data());

                        CHARRANGE crNow = {};
                        SendMessageW(hWnd, EM_EXGETSEL, 0, (LPARAM)&crNow);

                        CHARRANGE crReplace = { self->wordStartIndex_, crNow.cpMax };
                        SendMessageW(hWnd, EM_EXSETSEL, 0, (LPARAM)&crReplace);
                        SendMessageW(hWnd, EM_REPLACESEL, TRUE, (LPARAM)chosen.data());
                    }
                    ShowWindow(self->hwndPopup_, SW_HIDE);
                    return 0;
                }
                if (wParam == VK_ESCAPE)
                {
                    self->HideCallTip();
                    ShowWindow(self->hwndPopup_, SW_HIDE);
                    return 0;
                }
            }
            break;
        }
        case WM_NCDESTROY:
        {
            RemoveWindowSubclass(hWnd, RichEditSubclassProc, uIdSubclass);
            break;
        }
        }

        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_CREATE:
        {
            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
            LoadLibrary(TEXT("Msftedit.dll"));
            hwndEdit_ = CreateWindowExW(
                0, MSFTEDIT_CLASS, nullptr,
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
                0, 0, 0, 0, hwnd, (HMENU)102, hInst, nullptr
            );
            hwndGutter_ = CreateWindowExW(
                0, L"STATIC", nullptr,
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                0, 0, IDE_GUTTER_WIDTH, 0, hwnd, (HMENU)101, hInst, nullptr
            );
            hwndPopup_ = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                L"LISTBOX", L"",
                WS_POPUP | WS_BORDER | LBS_NOTIFY | LBS_HASSTRINGS | WS_VSCROLL,
                0, 0, 180, 120,
                hwnd, nullptr, hInst, nullptr
            );
            CreateCallTipTooltip(hInst);
            hEditorFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
            SendMessageW(hwndEdit_, WM_SETFONT, (WPARAM)hEditorFont, TRUE);
            SendMessageW(hwndPopup_, WM_SETFONT, (WPARAM)hEditorFont, TRUE);
            SendMessageW(hwndEdit_, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_UPDATE | ENM_SCROLL);
            SendMessageW(hwndEdit_, EM_SETOPTIONS, ECOOP_OR, ECO_SELECTIONBAR);
            int tabStop = 4 * 4;
            SendMessageW(hwndEdit_, EM_SETTABSTOPS, 1, (LPARAM)&tabStop);
            SetWindowSubclass(hwndEdit_, RichEditSubclassProc, 100, (DWORD_PTR)this);
            SetWindowSubclass(hwndGutter_, GutterSubclassProc, 101, (DWORD_PTR)this);
            ApplyThemeToRichEdit(hwndEdit_);
            return 0;
        }
        case WM_COMMAND:
        {
            if ((HWND)lParam == hwndEdit_) {
                WORD code = HIWORD(wParam);
                if (code == EN_CHANGE || code == EN_UPDATE) {
                    if (lintEnabled_) SetTimer(hwnd, TIMER_LINT_ID, 5, nullptr);
                    PostMessageW(hwnd, WM_GUTTERED_TEXT_EDITOR_REFRESH_GUTTER, 0, 0);
                    UpdateAutocomplete();
                }
                else if (code == EN_VSCROLL) {
                    PostMessageW(hwnd, WM_GUTTERED_TEXT_EDITOR_REFRESH_GUTTER, 0, 0);
                }
            }
            break;
        }
        case WM_TIMER:
        {
            if (wParam == TIMER_LINT_ID)
            {
                KillTimer(hwnd, TIMER_LINT_ID);
                if (!lintEnabled_)
                    return 0;

                int textLength = GetWindowTextLengthW(hwndEdit_);

                if (textLength == 0)
                {
                    g_errorLine = -1;
                    g_errorMessage = L"";
                    lastCompletedLintHash_ = 0;
                    latestLintHash_ = 0;
                    HighlightSyntax();
                    if (hwndGutter_) InvalidateRect(hwndGutter_, nullptr, FALSE);
                    return 0;
                }

                std::vector<wchar_t> buffer(textLength + 1);
                GetWindowTextW(hwndEdit_, buffer.data(), textLength + 1);
                std::wstring sourceCode = buffer.data();

                uint64_t currentHash = HashString64(sourceCode);

                if (currentHash == lastCompletedLintHash_)
                    return 0;

                latestLintHash_ = currentHash;

                std::thread([this, hwnd, sourceCode, currentHash]() {
                    CompileResult res = CompileCToMlog(sourceCode);

                    if (this->latestLintHash_.load() != currentHash)
                        return;

                    auto* result = new LintResult{
                        currentHash,
                        res.success,
                        res.errorLine,
                        res.error
                    };

                    PostMessageW(hwnd, WM_APP_LINT_DONE, reinterpret_cast<WPARAM>(result), 0);
                    }).detach();

                return 0;
            }
            break;
        }
        case WM_APP_LINT_DONE:
        {
            auto* result = reinterpret_cast<LintResult*>(wParam);
            if (result)
            {
                if (result->hash == latestLintHash_.load())
                {
                    lastCompletedLintHash_ = result->hash;

                    if (!result->success)
                    {
                        g_errorLine = result->errorLine;
                        g_errorMessage = result->error;
                    }
                    else
                    {
                        g_errorLine = -1;
                        g_errorMessage = L"";
                    }

                    HighlightSyntax();

                    if (hwndGutter_)
                        InvalidateRect(hwndGutter_, nullptr, FALSE);
                }

                delete result;
            }
            return 0;
        }
        case WM_GUTTERED_TEXT_EDITOR_REFRESH_GUTTER:
            if (hwndGutter_)
                InvalidateRect(hwndGutter_, nullptr, FALSE);
            return 0;
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            const int hScrollHeight = GetSystemMetrics(SM_CYHSCROLL);

            SCROLLINFO si = { sizeof(si), SIF_RANGE | SIF_PAGE };
            bool hasHScroll = false;
            if (GetScrollInfo(hwndEdit_, SB_HORZ, &si))
            {
                hasHScroll = (si.nMax >= (int)si.nPage);
            }

            int gutterHeight = hasHScroll ? (height - hScrollHeight) : height;

            MoveWindow(hwndEdit_, 0, 0, width, height, TRUE);
            MoveWindow(hwndGutter_, 0, 0, IDE_GUTTER_WIDTH, gutterHeight, TRUE);

            RECT rcFormat;
            GetClientRect(hwndEdit_, &rcFormat);
            rcFormat.left = IDE_GUTTER_WIDTH + 10;
            SendMessageW(hwndEdit_, EM_SETRECT, 0, (LPARAM)&rcFormat);

            return 0;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            hwnd_ = nullptr;
            if (hwndPopup_)
            {
                DestroyWindow(hwndPopup_);
                hwndPopup_ = nullptr;
            }
            if (hwndGutter_)
            {
                DestroyWindow(hwndGutter_);
                hwndGutter_ = nullptr;
            }
            if (hwndEdit_)
            {
                DestroyWindow(hwndEdit_);
                hwndEdit_ = nullptr;
            }
            if (hwndTooltip_) {
                DestroyWindow(hwndTooltip_);
                hwndTooltip_ = nullptr;
            }
            (hEditorFont) ? DeleteObject(hEditorFont) : 0;
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
};