/**
 * cef_browser.cpp
 * 
 * Modern ICMP Browser with CEF (Chromium Embedded Framework)
 * Custom modern UI with full Chromium rendering
 */

#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <sstream>

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_command_line.h"

// Include ICMP tunnel
extern "C" {
#include "../client/icmp_engine.h"
#include "../common/protocol.h"
#include "../common/handshake.h"
}

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ws2_32.lib")

// Control IDs
#define ID_URL_BAR      1001
#define ID_GO_BUTTON    1002
#define ID_BACK_BUTTON  1003
#define ID_FWD_BUTTON   1004
#define ID_REFRESH_BTN  1005
#define ID_SETTINGS_BTN 1006

// Modern colors
#define COLOR_BG        RGB(18, 18, 18)      // Dark background
#define COLOR_TOOLBAR   RGB(32, 32, 32)      // Dark toolbar
#define COLOR_BUTTON    RGB(45, 45, 45)      // Button background
#define COLOR_ACCENT    RGB(0, 120, 212)     // Accent blue
#define COLOR_TEXT      RGB(255, 255, 255)   // White text
#define COLOR_BORDER    RGB(60, 60, 60)      // Border

// Global state
static HWND g_hwnd = NULL;
static HWND g_urlBar = NULL;
static HWND g_goBtn = NULL;
static HWND g_backBtn = NULL;
static HWND g_fwdBtn = NULL;
static HWND g_refreshBtn = NULL;
static HWND g_settingsBtn = NULL;
static CefRefPtr<CefBrowser> g_browser = NULL;
static std::string g_serverIp = "192.168.1.40";
static icmp_session_t* g_session = NULL;
static uint8_t g_sessionKey[32];
static bool g_handshakeComplete = false;

// Fonts and brushes
static HFONT g_fontNormal = NULL;
static HFONT g_fontIcon = NULL;
static HBRUSH g_brushBg = NULL;
static HBRUSH g_brushToolbar = NULL;
static HBRUSH g_brushButton = NULL;

// Forward declarations
void CreateModernUI(HWND hwnd);
void ResizeControls();
void OnNavigate();
void OnBack();
void OnForward();
void OnRefresh();
void OnSettings();
std::string FetchViaICMP(const std::string& url);

// CEF Client Handler
class BrowserClient : public CefClient,
                      public CefLifeSpanHandler,
                      public CefLoadHandler,
                      public CefDisplayHandler {
public:
    BrowserClient() {}

    // CefClient methods
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }

    virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override {
        return this;
    }

    virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
        return this;
    }

    // CefLifeSpanHandler methods
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        g_browser = browser;
    }

    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        g_browser = nullptr;
    }

    // CefLoadHandler methods
    virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            TransitionType transition_type) override {
        // Update URL bar
        if (frame->IsMain()) {
            std::string url = frame->GetURL();
            SetWindowTextA(g_urlBar, url.c_str());
        }
    }

    virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          int httpStatusCode) override {
        // Update navigation buttons
        EnableWindow(g_backBtn, browser->CanGoBack());
        EnableWindow(g_fwdBtn, browser->CanGoForward());
    }

    // CefDisplayHandler methods
    virtual void OnTitleChange(CefRefPtr<CefBrowser> browser,
                              const CefString& title) override {
        std::wstring titleStr = title.ToWString();
        SetWindowTextW(g_hwnd, (L"ICMP Browser - " + titleStr).c_str());
    }

private:
    IMPLEMENT_REFCOUNTING(BrowserClient);
};

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            CreateModernUI(hwnd);
            return 0;

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
                case ID_GO_BUTTON:
                    OnNavigate();
                    return 0;
                case ID_BACK_BUTTON:
                    OnBack();
                    return 0;
                case ID_FWD_BUTTON:
                    OnForward();
                    return 0;
                case ID_REFRESH_BTN:
                    OnRefresh();
                    return 0;
                case ID_SETTINGS_BTN:
                    OnSettings();
                    return 0;
            }
            break;
        }

        case WM_SIZE:
            ResizeControls();
            if (g_browser) {
                RECT rect;
                GetClientRect(hwnd, &rect);
                rect.top = 56; // Toolbar height
                HWND browserHwnd = g_browser->GetHost()->GetWindowHandle();
                SetWindowPos(browserHwnd, NULL,
                    rect.left, rect.top,
                    rect.right - rect.left,
                    rect.bottom - rect.top,
                    SWP_NOZORDER);
            }
            return 0;

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, COLOR_TEXT);
            SetBkColor(hdc, COLOR_BUTTON);
            return (LRESULT)g_brushButton;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, COLOR_TEXT);
            SetBkColor(hdc, COLOR_TOOLBAR);
            return (LRESULT)g_brushToolbar;
        }

        case WM_CLOSE:
            if (g_browser) {
                g_browser->GetHost()->CloseBrowser(false);
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CreateModernUI(HWND hwnd) {
    // Create fonts
    g_fontNormal = CreateFont(
        -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    g_fontIcon = CreateFont(
        -20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI Symbol"
    );

    // Create brushes
    g_brushBg = CreateSolidBrush(COLOR_BG);
    g_brushToolbar = CreateSolidBrush(COLOR_TOOLBAR);
    g_brushButton = CreateSolidBrush(COLOR_BUTTON);

    int margin = 12;
    int btnSize = 40;
    int y = 8;
    int x = margin;

    // Back button
    g_backBtn = CreateWindowW(L"BUTTON", L"◄",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
        x, y, btnSize, btnSize,
        hwnd, (HMENU)ID_BACK_BUTTON, NULL, NULL);
    SendMessage(g_backBtn, WM_SETFONT, (WPARAM)g_fontIcon, TRUE);
    x += btnSize + 6;

    // Forward button
    g_fwdBtn = CreateWindowW(L"BUTTON", L"►",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
        x, y, btnSize, btnSize,
        hwnd, (HMENU)ID_FWD_BUTTON, NULL, NULL);
    SendMessage(g_fwdBtn, WM_SETFONT, (WPARAM)g_fontIcon, TRUE);
    x += btnSize + 6;

    // Refresh button
    g_refreshBtn = CreateWindowW(L"BUTTON", L"↻",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
        x, y, btnSize, btnSize,
        hwnd, (HMENU)ID_REFRESH_BTN, NULL, NULL);
    SendMessage(g_refreshBtn, WM_SETFONT, (WPARAM)g_fontIcon, TRUE);
    x += btnSize + margin * 2;

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    // URL bar
    int rightMargin = margin + 100 + margin + btnSize + margin;
    int urlBarWidth = rcClient.right - x - rightMargin;
    g_urlBar = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
        x, y, urlBarWidth, btnSize,
        hwnd, (HMENU)ID_URL_BAR, NULL, NULL);
    SendMessage(g_urlBar, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    x += urlBarWidth + margin;

    // GO button
    g_goBtn = CreateWindowW(L"BUTTON", L"GO",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
        x, y, 100, btnSize,
        hwnd, (HMENU)ID_GO_BUTTON, NULL, NULL);
    SendMessage(g_goBtn, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    x += 100 + margin;

    // Settings button
    g_settingsBtn = CreateWindowW(L"BUTTON", L"⚙",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT,
        x, y, btnSize, btnSize,
        hwnd, (HMENU)ID_SETTINGS_BTN, NULL, NULL);
    SendMessage(g_settingsBtn, WM_SETFONT, (WPARAM)g_fontIcon, TRUE);

    // Create CEF browser
    CefWindowInfo window_info;
    GetClientRect(hwnd, &rcClient);
    RECT rect = rcClient;
    rect.top = 56;
    window_info.SetAsChild(hwnd, rect);
    window_info.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

    CefBrowserSettings browser_settings;
    browser_settings.background_color = CefColorSetARGB(255, 18, 18, 18);

    CefRefPtr<BrowserClient> client = new BrowserClient();

    CefBrowserHost::CreateBrowser(
        window_info,
        client.get(),
        "about:blank",
        browser_settings,
        nullptr,
        nullptr
    );
}

void ResizeControls() {
    RECT rcClient;
    GetClientRect(g_hwnd, &rcClient);

    int margin = 12;
    int btnSize = 40;
    int y = 8;
    int x = margin;

    SetWindowPos(g_backBtn, NULL, x, y, btnSize, btnSize, SWP_NOZORDER);
    x += btnSize + 6;

    SetWindowPos(g_fwdBtn, NULL, x, y, btnSize, btnSize, SWP_NOZORDER);
    x += btnSize + 6;

    SetWindowPos(g_refreshBtn, NULL, x, y, btnSize, btnSize, SWP_NOZORDER);
    x += btnSize + margin * 2;

    int rightMargin = margin + 100 + margin + btnSize + margin;
    int urlBarWidth = rcClient.right - x - rightMargin;
    SetWindowPos(g_urlBar, NULL, x, y, urlBarWidth, btnSize, SWP_NOZORDER);
    x += urlBarWidth + margin;

    SetWindowPos(g_goBtn, NULL, x, y, 100, btnSize, SWP_NOZORDER);
    x += 100 + margin;

    SetWindowPos(g_settingsBtn, NULL, x, y, btnSize, btnSize, SWP_NOZORDER);
}

void OnNavigate() {
    char url[2048];
    GetWindowTextA(g_urlBar, url, 2048);

    std::string urlStr(url);
    if (urlStr.empty()) return;

    // Add http:// if needed
    if (urlStr.find("://") == std::string::npos) {
        urlStr = "http://" + urlStr;
        SetWindowTextA(g_urlBar, urlStr.c_str());
    }

    // Fetch via ICMP and load in CEF
    std::string html = FetchViaICMP(urlStr);
    if (!html.empty() && g_browser) {
        std::string dataUrl = "data:text/html;charset=utf-8," + html;
        g_browser->GetMainFrame()->LoadURL(dataUrl);
    }
}

void OnBack() {
    if (g_browser && g_browser->CanGoBack()) {
        g_browser->GoBack();
    }
}

void OnForward() {
    if (g_browser && g_browser->CanGoForward()) {
        g_browser->GoForward();
    }
}

void OnRefresh() {
    OnNavigate(); // Re-fetch via ICMP
}

void OnSettings() {
    MessageBoxA(g_hwnd,
        ("Server: " + g_serverIp).c_str(),
        "Settings",
        MB_OK | MB_ICONINFORMATION);
}

std::string FetchViaICMP(const std::string& url) {
    // Parse URL to get host
    std::string host;
    std::string path = "/";
    
    size_t proto_end = url.find("://");
    if (proto_end != std::string::npos) {
        size_t host_start = proto_end + 3;
        size_t path_start = url.find('/', host_start);
        
        if (path_start != std::string::npos) {
            host = url.substr(host_start, path_start - host_start);
            path = url.substr(path_start);
        } else {
            host = url.substr(host_start);
        }
    } else {
        host = url;
    }
    
    // Create HTTP request
    std::ostringstream oss;
    oss << "GET " << path << " HTTP/1.1\r\n";
    oss << "Host: " << host << "\r\n";
    oss << "Connection: close\r\n";
    oss << "User-Agent: ICMP-Browser/1.0\r\n";
    oss << "\r\n";
    
    std::string http_request = oss.str();
    
    // Create IP packet with HTTP request (simplified - real implementation would be more complex)
    uint8_t request_data[2048];
    memcpy(request_data, http_request.c_str(), http_request.size());
    
    // TODO: Send via ICMP tunnel and get response
    // For now, return a placeholder
    
    return "<html><body><h1>ICMP Fetch</h1><p>URL: " + url + "</p><p>Host: " + host + "</p><p>Path: " + path + "</p><p><i>ICMP tunnel integration in progress...</i></p></body></html>";
}

// CEF App
class BrowserApp : public CefApp {
public:
    BrowserApp() {}

private:
    IMPLEMENT_REFCOUNTING(BrowserApp);
};

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nCmdShow);

    // Initialize CEF
    CefMainArgs main_args(hInstance);
    CefRefPtr<BrowserApp> app(new BrowserApp);

    int exit_code = CefExecuteProcess(main_args, app.get(), nullptr);
    if (exit_code >= 0) {
        return exit_code;
    }

    CefSettings settings;
    settings.no_sandbox = true;
    settings.multi_threaded_message_loop = true;

    CefInitialize(main_args, settings, app.get(), nullptr);

    // Initialize Winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Register window class
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = g_brushBg;
    wc.lpszClassName = L"ICMPBrowserCEF";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    // Create window
    g_hwnd = CreateWindowW(
        L"ICMPBrowserCEF",
        L"ICMP Browser",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 800,
        NULL, NULL, hInstance, NULL
    );

    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    CefShutdown();
    WSACleanup();

    return (int)msg.wParam;
}
