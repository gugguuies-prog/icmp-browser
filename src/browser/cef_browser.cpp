/**
 * cef_browser.cpp
 * 
 * Modern ICMP Browser with CEF (Chromium Embedded Framework)
 * Custom modern UI with full Chromium rendering
 */

#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <time.h>

// Undefine Windows macros that conflict with CEF BEFORE including CEF headers
#ifdef GetNextSibling
#undef GetNextSibling
#endif
#ifdef GetFirstChild
#undef GetFirstChild
#endif
#ifdef GetPrevSibling
#undef GetPrevSibling
#endif
#ifdef GetParent
#undef GetParent
#endif

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
#include "../crypto/aes_gcm.h"
#include "../crypto/diffie_hellman.h"
}

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

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
static CefRefPtr<CefBrowser> g_browser = nullptr;
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
        // Post quit message to close the application
        PostQuitMessage(0);
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
            // Cleanup ICMP session
            if (g_session) {
                icmp_session_destroy(g_session);
                g_session = NULL;
                g_handshakeComplete = false;
            }
            
            if (g_browser) {
                g_browser->GetHost()->CloseBrowser(true);  // Force close
            } else {
                DestroyWindow(hwnd);
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
    CefRect cef_rect(0, 56, rcClient.right, rcClient.bottom);
    window_info.SetAsChild(hwnd, cef_rect);
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
    char buffer[256];
    sprintf_s(buffer, sizeof(buffer), 
             "Current Server: %s\n\n"
             "Session: %s\n"
             "Packets Sent: %u\n"
             "Packets Received: %u",
             g_serverIp.c_str(),
             g_handshakeComplete ? "Connected" : "Not connected",
             g_session ? g_session->packets_sent : 0,
             g_session ? g_session->packets_received : 0);
    
    MessageBoxA(g_hwnd, buffer, "ICMP Browser Settings", MB_OK | MB_ICONINFORMATION);
}

// Helper functions for ICMP communication
static bool InitializeICMPSession() {
    if (g_session != NULL) {
        return true; // Already initialized
    }
    
    // Create ICMP session
    g_session = icmp_session_create(g_serverIp.c_str());
    if (!g_session) {
        MessageBoxA(g_hwnd, "Failed to create ICMP session", "Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    // Perform Diffie-Hellman handshake
    dh_context_t dh_ctx;
    if (dh_init(&dh_ctx) != 0) {
        MessageBoxA(g_hwnd, "Failed to initialize Diffie-Hellman", "Error", MB_OK | MB_ICONERROR);
        icmp_session_destroy(g_session);
        g_session = NULL;
        return false;
    }
    
    uint8_t public_key[DH_PUBLIC_KEY_SIZE];
    if (dh_generate_keypair(&dh_ctx, public_key, DH_PUBLIC_KEY_SIZE) != 0) {
        MessageBoxA(g_hwnd, "Failed to generate DH keypair", "Error", MB_OK | MB_ICONERROR);
        dh_cleanup(&dh_ctx);
        icmp_session_destroy(g_session);
        g_session = NULL;
        return false;
    }
    
    // Create handshake packet
    uint8_t handshake_packet[512];
    size_t packet_size = create_handshake_packet(public_key, DH_PUBLIC_KEY_SIZE,
                                                  handshake_packet, sizeof(handshake_packet));
    
    if (packet_size == 0) {
        MessageBoxA(g_hwnd, "Failed to create handshake packet", "Error", MB_OK | MB_ICONERROR);
        dh_cleanup(&dh_ctx);
        icmp_session_destroy(g_session);
        g_session = NULL;
        return false;
    }
    
    // Send handshake
    uint8_t reply[512];
    size_t reply_size = 0;
    int result = icmp_send_reliable(g_session, handshake_packet, packet_size, reply, &reply_size);
    
    if (result != ICMP_SUCCESS) {
        char error[256];
        sprintf_s(error, "Handshake failed: %s", icmp_error_string(result));
        MessageBoxA(g_hwnd, error, "Error", MB_OK | MB_ICONERROR);
        dh_cleanup(&dh_ctx);
        icmp_session_destroy(g_session);
        g_session = NULL;
        return false;
    }
    
    // Parse handshake response
    uint8_t server_public_key[DH_PUBLIC_KEY_SIZE];
    if (parse_handshake_response(reply, reply_size, server_public_key, DH_PUBLIC_KEY_SIZE) != 0) {
        MessageBoxA(g_hwnd, "Failed to parse handshake response", "Error", MB_OK | MB_ICONERROR);
        dh_cleanup(&dh_ctx);
        icmp_session_destroy(g_session);
        g_session = NULL;
        return false;
    }
    
    // Compute shared secret
    if (dh_compute_shared_secret(&dh_ctx, server_public_key, DH_PUBLIC_KEY_SIZE,
                                  g_sessionKey, sizeof(g_sessionKey)) != 0) {
        MessageBoxA(g_hwnd, "Failed to compute shared secret", "Error", MB_OK | MB_ICONERROR);
        dh_cleanup(&dh_ctx);
        icmp_session_destroy(g_session);
        g_session = NULL;
        return false;
    }
    
    dh_cleanup(&dh_ctx);
    g_handshakeComplete = true;
    
    return true;
}

static std::string FetchViaICMP(const std::string& url) {
    // Initialize ICMP session if needed
    if (!InitializeICMPSession()) {
        return "<html><body><h1>Error</h1><p>Failed to initialize ICMP tunnel</p></body></html>";
    }
    
    // Parse URL to get host and path
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
    oss << "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
    oss << "\r\n";
    
    std::string http_request = oss.str();
    
    // Create simple IP packet structure (minimal for demonstration)
    // In reality, you'd need full IP/TCP header construction
    uint8_t request_packet[2048];
    size_t request_size = http_request.size();
    
    if (request_size > sizeof(request_packet)) {
        return "<html><body><h1>Error</h1><p>Request too large</p></body></html>";
    }
    
    memcpy(request_packet, http_request.c_str(), request_size);
    
    // Encrypt the packet
    uint8_t encrypted_packet[2048];
    size_t encrypted_size = 0;
    
    // Use session ID and sequence for AAD
    uint8_t aad[4];
    aad[0] = (g_session->session_id >> 8) & 0xFF;
    aad[1] = g_session->session_id & 0xFF;
    aad[2] = (g_session->sequence_counter >> 8) & 0xFF;
    aad[3] = g_session->sequence_counter & 0xFF;
    
    uint8_t iv[12];
    for (int i = 0; i < 12; i++) {
        iv[i] = rand() % 256;
    }
    
    uint8_t tag[16];
    
    if (aes_gcm_encrypt(g_sessionKey, sizeof(g_sessionKey),
                        iv, sizeof(iv),
                        aad, sizeof(aad),
                        request_packet, request_size,
                        encrypted_packet, &encrypted_size,
                        tag, sizeof(tag)) != 0) {
        return "<html><body><h1>Error</h1><p>Encryption failed</p></body></html>";
    }
    
    // Create protocol packet
    uint8_t protocol_packet[2048];
    protocol_header_t* header = (protocol_header_t*)protocol_packet;
    header->version = PROTOCOL_VERSION;
    header->type = PACKET_TYPE_DATA;
    header->flags = 0;
    header->session_id = htons(g_session->session_id);
    header->sequence = htons(g_session->sequence_counter);
    header->payload_length = htons((uint16_t)encrypted_size);
    
    // Copy IV, tag, and encrypted data
    memcpy(header->iv, iv, sizeof(iv));
    memcpy(header->tag, tag, sizeof(tag));
    memcpy(protocol_packet + sizeof(protocol_header_t), encrypted_packet, encrypted_size);
    
    size_t total_size = sizeof(protocol_header_t) + encrypted_size;
    
    // Send via ICMP
    uint8_t response[8192]; // Larger buffer for HTTP response
    size_t response_size = 0;
    
    int result = icmp_send_reliable(g_session, protocol_packet, total_size, response, &response_size);
    
    if (result != ICMP_SUCCESS) {
        char error[512];
        sprintf_s(error, sizeof(error), 
                 "<html><body><h1>Error</h1><p>ICMP send failed: %s</p><p>URL: %s</p></body></html>",
                 icmp_error_string(result), url.c_str());
        return std::string(error);
    }
    
    if (response_size < sizeof(protocol_header_t)) {
        return "<html><body><h1>Error</h1><p>Invalid response (too small)</p></body></html>";
    }
    
    // Parse response
    protocol_header_t* resp_header = (protocol_header_t*)response;
    
    if (resp_header->version != PROTOCOL_VERSION) {
        return "<html><body><h1>Error</h1><p>Invalid protocol version</p></body></html>";
    }
    
    if (resp_header->type != PACKET_TYPE_DATA) {
        return "<html><body><h1>Error</h1><p>Invalid packet type</p></body></html>";
    }
    
    uint16_t payload_len = ntohs(resp_header->payload_length);
    
    if (response_size < sizeof(protocol_header_t) + payload_len) {
        return "<html><body><h1>Error</h1><p>Invalid response (payload truncated)</p></body></html>";
    }
    
    // Decrypt response
    uint8_t decrypted[8192];
    size_t decrypted_size = 0;
    
    // Reconstruct AAD for response
    uint8_t resp_aad[4];
    resp_aad[0] = (ntohs(resp_header->session_id) >> 8) & 0xFF;
    resp_aad[1] = ntohs(resp_header->session_id) & 0xFF;
    resp_aad[2] = (ntohs(resp_header->sequence) >> 8) & 0xFF;
    resp_aad[3] = ntohs(resp_header->sequence) & 0xFF;
    
    if (aes_gcm_decrypt(g_sessionKey, sizeof(g_sessionKey),
                        resp_header->iv, sizeof(resp_header->iv),
                        resp_aad, sizeof(resp_aad),
                        response + sizeof(protocol_header_t), payload_len,
                        resp_header->tag, sizeof(resp_header->tag),
                        decrypted, &decrypted_size) != 0) {
        return "<html><body><h1>Error</h1><p>Decryption failed</p></body></html>";
    }
    
    // Extract HTML from HTTP response
    std::string http_response((char*)decrypted, decrypted_size);
    
    // Find the end of HTTP headers (double CRLF)
    size_t body_start = http_response.find("\r\n\r\n");
    if (body_start == std::string::npos) {
        body_start = http_response.find("\n\n");
        if (body_start != std::string::npos) {
            body_start += 2;
        }
    } else {
        body_start += 4;
    }
    
    if (body_start != std::string::npos && body_start < http_response.length()) {
        return http_response.substr(body_start);
    }
    
    // If no body separator found, return the whole response
    return http_response;
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
    
    // Get server IP from command line
    if (__argc > 1) {
        g_serverIp = __argv[1];
    }
    
    // Seed random for IV generation
    srand((unsigned int)time(NULL));

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
    if (g_session) {
        icmp_session_destroy(g_session);
        g_session = NULL;
    }
    
    CefShutdown();
    WSACleanup();

    return (int)msg.wParam;
}
