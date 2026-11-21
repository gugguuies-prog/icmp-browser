/**
 * cef_browser.cpp
 * 
 * Modern ICMP Browser with CEF (Chromium Embedded Framework)
 * Custom modern UI with full Chromium rendering
 */

#include <string>
#include <sstream>

// Include CEF headers BEFORE Windows headers to avoid macro conflicts
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_command_line.h"

// Now include Windows headers
#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <time.h>

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

// Modern colors - Light theme
#define COLOR_BG        RGB(255, 255, 255)   // White background
#define COLOR_TOOLBAR   RGB(245, 245, 245)   // Light gray toolbar
#define COLOR_BUTTON    RGB(240, 240, 240)   // Light button background
#define COLOR_ACCENT    RGB(0, 120, 212)     // Accent blue
#define COLOR_TEXT      RGB(0, 0, 0)         // Black text
#define COLOR_BORDER    RGB(200, 200, 200)   // Light border

// Global state
static HWND g_hwnd = NULL;
static HWND g_urlBar = NULL;
static HWND g_goBtn = NULL;
static HWND g_backBtn = NULL;
static HWND g_fwdBtn = NULL;
static HWND g_refreshBtn = NULL;
static HWND g_settingsBtn = NULL;
static CefRefPtr<CefBrowser> g_browser;
static std::string g_serverIp = "192.168.1.40";
static icmp_session_t* g_session = NULL;
static uint8_t g_sessionKey[32];
static bool g_handshakeComplete = false;
static uint16_t g_dataSequence = 0;  // Separate sequence counter for DATA packets

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
        
        // Load welcome page
        std::string welcome_html = 
            "<html><head><style>"
            "body { background: #fff; color: #000; font-family: 'Segoe UI', sans-serif; "
            "       display: flex; align-items: center; justify-content: center; height: 100vh; margin: 0; }"
            "h1 { font-size: 48px; font-weight: 300; }"
            "p { font-size: 18px; color: #666; }"
            "</style></head><body>"
            "<div style='text-align: center;'>"
            "<h1>ICMP Browser</h1>"
            "<p>Enter a URL in the address bar to start browsing</p>"
            "<p style='font-size: 14px; margin-top: 40px; color: #999;'>Connected to: " + g_serverIp + "</p>"
            "</div></body></html>";
        
        browser->GetMainFrame()->LoadURL("data:text/html;charset=utf-8," + welcome_html);
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
        // Update URL bar only for real URLs (not data: URLs from our fetch)
        if (frame->IsMain()) {
            std::string url = frame->GetURL();
            // Don't update URL bar for data: URLs (our fetched content)
            if (url.find("data:") != 0) {
                SetWindowTextA(g_urlBar, url.c_str());
            }
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
            if (g_browser.get()) {
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
                g_dataSequence = 0;
            }
            
            if (g_browser.get()) {
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
    g_backBtn = CreateWindowW(L"BUTTON", L"<",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, btnSize, btnSize,
        hwnd, (HMENU)ID_BACK_BUTTON, NULL, NULL);
    SendMessage(g_backBtn, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    x += btnSize + 6;

    // Forward button
    g_fwdBtn = CreateWindowW(L"BUTTON", L">",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, btnSize, btnSize,
        hwnd, (HMENU)ID_FWD_BUTTON, NULL, NULL);
    SendMessage(g_fwdBtn, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
    x += btnSize + 6;

    // Refresh button
    g_refreshBtn = CreateWindowW(L"BUTTON", L"R",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, btnSize, btnSize,
        hwnd, (HMENU)ID_REFRESH_BTN, NULL, NULL);
    SendMessage(g_refreshBtn, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
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
    g_settingsBtn = CreateWindowW(L"BUTTON", L"...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, btnSize, btnSize,
        hwnd, (HMENU)ID_SETTINGS_BTN, NULL, NULL);
    SendMessage(g_settingsBtn, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);

    // Create CEF browser
    CefWindowInfo window_info;
    GetClientRect(hwnd, &rcClient);
    CefRect cef_rect(0, 56, rcClient.right, rcClient.bottom);
    window_info.SetAsChild(hwnd, cef_rect);
    window_info.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

    CefBrowserSettings browser_settings;
    browser_settings.background_color = CefColorSetARGB(255, 255, 255, 255); // White background

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
    if (!html.empty() && g_browser.get()) {
        std::string dataUrl = "data:text/html;charset=utf-8," + html;
        g_browser->GetMainFrame()->LoadURL(dataUrl);
    }
}

void OnBack() {
    if (g_browser.get() && g_browser->CanGoBack()) {
        g_browser->GoBack();
    }
}

void OnForward() {
    if (g_browser.get() && g_browser->CanGoForward()) {
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
    // Return true if already connected
    if (g_session != NULL && g_handshakeComplete) {
        return true;
    }
    
    // Destroy old session if exists
    if (g_session != NULL) {
        icmp_session_destroy(g_session);
        g_session = NULL;
        g_handshakeComplete = false;
        g_dataSequence = 0;
    }
    
    // Create ICMP session
    g_session = icmp_session_create(g_serverIp.c_str());
    if (!g_session) {
        MessageBoxA(g_hwnd, "Failed to create ICMP session", "Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    // Perform Diffie-Hellman handshake
    handshake_context_t hs_ctx;
    if (handshake_init(&hs_ctx, g_session->session_id) != 0) {
        MessageBoxA(g_hwnd, "Failed to initialize handshake", "Error", MB_OK | MB_ICONERROR);
        icmp_session_destroy(g_session);
        g_session = NULL;
        return false;
    }
    
    // Create handshake request
    proto_packet_t request;
    if (handshake_client_start(&hs_ctx, &request) != 0) {
        MessageBoxA(g_hwnd, "Failed to create handshake request", "Error", MB_OK | MB_ICONERROR);
        handshake_cleanup(&hs_ctx);
        icmp_session_destroy(g_session);
        g_session = NULL;
        return false;
    }
    
    // Prepare packet for sending (header + payload)
    uint8_t send_buffer[2048];
    memcpy(send_buffer, &request.header, sizeof(proto_header_t));
    memcpy(send_buffer + sizeof(proto_header_t), request.payload, request.payload_size);
    size_t send_size = sizeof(proto_header_t) + request.payload_size;
    
    // Send handshake
    uint8_t reply_buffer[2048];
    size_t reply_size = 0;
    int result = icmp_send_reliable(g_session, send_buffer, send_size, reply_buffer, &reply_size);
    
    if (result != ICMP_SUCCESS) {
        char error[256];
        sprintf_s(error, "Handshake failed: %s", icmp_error_string(result));
        MessageBoxA(g_hwnd, error, "Error", MB_OK | MB_ICONERROR);
        handshake_cleanup(&hs_ctx);
        icmp_session_destroy(g_session);
        g_session = NULL;
        return false;
    }
    
    // Parse handshake response
    if (reply_size < sizeof(proto_header_t)) {
        MessageBoxA(g_hwnd, "Invalid handshake response", "Error", MB_OK | MB_ICONERROR);
        handshake_cleanup(&hs_ctx);
        icmp_session_destroy(g_session);
        g_session = NULL;
        return false;
    }
    
    proto_packet_t response;
    memcpy(&response.header, reply_buffer, sizeof(proto_header_t));
    response.payload_size = reply_size - sizeof(proto_header_t);
    if (response.payload_size > 0) {
        memcpy(response.payload, reply_buffer + sizeof(proto_header_t), response.payload_size);
    }
    
    // Debug: dump first 16 bytes of reply_buffer
    char hex_dump[512];
    sprintf_s(hex_dump, sizeof(hex_dump),
             "Reply buffer (first 16 bytes): %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n"
             "reply_size=%zu, header_size=%zu, payload_size=%zu",
             reply_buffer[0], reply_buffer[1], reply_buffer[2], reply_buffer[3],
             reply_buffer[4], reply_buffer[5], reply_buffer[6], reply_buffer[7],
             reply_buffer[8], reply_buffer[9], reply_buffer[10], reply_buffer[11],
             reply_buffer[12], reply_buffer[13], reply_buffer[14], reply_buffer[15],
             reply_size, sizeof(proto_header_t), response.payload_size);
    OutputDebugStringA(hex_dump);
    
    // Complete handshake
    if (handshake_client_finish(&hs_ctx, &response) != 0) {
        MessageBoxA(g_hwnd, "Failed to complete handshake", "Error", MB_OK | MB_ICONERROR);
        handshake_cleanup(&hs_ctx);
        icmp_session_destroy(g_session);
        g_session = NULL;
        return false;
    }
    
    // Copy session key
    memcpy(g_sessionKey, hs_ctx.session_key, sizeof(g_sessionKey));
    
    // Debug: log public keys and session key
    char debug[1024];
    sprintf_s(debug, sizeof(debug),
             "Handshake complete!\n"
             "Client pubkey (first 8): %02X %02X %02X %02X %02X %02X %02X %02X\n"
             "Server pubkey (first 8): %02X %02X %02X %02X %02X %02X %02X %02X\n"
             "Session key (first 8): %02X %02X %02X %02X %02X %02X %02X %02X",
             hs_ctx.client_public_key[0], hs_ctx.client_public_key[1], hs_ctx.client_public_key[2], hs_ctx.client_public_key[3],
             hs_ctx.client_public_key[4], hs_ctx.client_public_key[5], hs_ctx.client_public_key[6], hs_ctx.client_public_key[7],
             hs_ctx.server_public_key[0], hs_ctx.server_public_key[1], hs_ctx.server_public_key[2], hs_ctx.server_public_key[3],
             hs_ctx.server_public_key[4], hs_ctx.server_public_key[5], hs_ctx.server_public_key[6], hs_ctx.server_public_key[7],
             g_sessionKey[0], g_sessionKey[1], g_sessionKey[2], g_sessionKey[3],
             g_sessionKey[4], g_sessionKey[5], g_sessionKey[6], g_sessionKey[7]);
    OutputDebugStringA(debug);
    
    handshake_cleanup(&hs_ctx);
    g_handshakeComplete = true;
    g_dataSequence = 0;  // Reset data sequence counter for new session
    
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
    
    // Encapsulate HTTP request into protocol packets
    proto_packet_t packets[10];
    size_t packet_count = 0;
    
    uint16_t data_seq = g_dataSequence++;
    
    // Debug log
    char debug[512];
    sprintf_s(debug, sizeof(debug),
             "Sending request:\ndata_seq=%d\nsession_id=0x%04x\nkey=%02X %02X %02X %02X...\nrequest_size=%zu",
             data_seq, g_session->session_id,
             g_sessionKey[0], g_sessionKey[1], g_sessionKey[2], g_sessionKey[3],
             http_request.size());
    OutputDebugStringA(debug);
    
    int result = proto_encapsulate((const uint8_t*)http_request.c_str(), http_request.size(),
                                   g_sessionKey, g_session->session_id,
                                   data_seq, packets, &packet_count, 10);
    
    if (result != PROTO_SUCCESS || packet_count == 0) {
        return "<html><body><h1>Error</h1><p>Failed to encapsulate request</p></body></html>";
    }
    
    // Send all packets and collect responses - WITH FRAGMENT HANDLING
    std::string html_response;
    proto_reassembly_ctx_t reassembly_ctx = {0};  // Fragment reassembly context
    
    for (size_t i = 0; i < packet_count; i++) {
        // Prepare packet for sending
        uint8_t send_buffer[2048];
        memcpy(send_buffer, &packets[i].header, sizeof(proto_header_t));
        memcpy(send_buffer + sizeof(proto_header_t), packets[i].payload, packets[i].payload_size);
        size_t send_size = sizeof(proto_header_t) + packets[i].payload_size;
        
        // Send via ICMP
        uint8_t reply_buffer[8192];
        size_t reply_size = 0;
        
        int send_result = icmp_send_reliable(g_session, send_buffer, send_size,
                                            reply_buffer, &reply_size);
        
        if (send_result != ICMP_SUCCESS) {
            char error[512];
            sprintf_s(error, sizeof(error), 
                     "<html><body><h1>Error</h1><p>ICMP send failed: %s</p></body></html>",
                     icmp_error_string(send_result));
            proto_reset_reassembly(&reassembly_ctx);
            return std::string(error);
        }
        
        if (reply_size < sizeof(proto_header_t)) {
            continue; // Skip invalid responses
        }
        
        // Parse ALL fragments from the response packet (server concatenates them)
        size_t offset = 0;
        
        OutputDebugStringA("Parsing fragments from combined packet...");
        
        while (offset + sizeof(proto_header_t) <= reply_size) {
            // Parse fragment header
            proto_packet_t frag;
            memcpy(&frag.header, reply_buffer + offset, sizeof(proto_header_t));
            offset += sizeof(proto_header_t);
            
            // Calculate payload size for this fragment
            // Each fragment has its own header+payload in the buffer
            size_t frag_payload_size = frag.header.fragment_num < (frag.header.total_fragments - 1) ? 
                                       PROTO_MAX_PAYLOAD : (reply_size - offset);
            
            if (frag_payload_size > PROTO_MAX_PAYLOAD) frag_payload_size = PROTO_MAX_PAYLOAD;
            
            frag.payload_size = frag_payload_size;
            
            if (frag.payload_size > 0 && offset + frag.payload_size <= reply_size) {
                memcpy(frag.payload, reply_buffer + offset, frag.payload_size);
                offset += frag.payload_size;
            } else {
                break; // No more complete fragments
            }
            
            // Log fragment
            char frag_log[256];
            sprintf_s(frag_log, sizeof(frag_log),
                     "Parsed fragment: frag_id=%d, frag_num=%d, total=%d, size=%zu",
                     frag.header.fragment_id, frag.header.fragment_num,
                     frag.header.total_fragments, frag.payload_size);
            OutputDebugStringA(frag_log);
            
            // Check if fragmented
            if (frag.header.flags & PROTO_FLAG_FRAGMENTED) {
                // Add to reassembly
                int frag_result = proto_add_fragment(&reassembly_ctx, &frag, g_sessionKey,
                                                     g_session->session_id, data_seq);
                
                if (frag_result > 0) {
                    // Reassembly complete!
                    uint8_t reassembled[65536];
                    size_t reassembled_size = 0;
                    
                    int get_result = proto_get_reassembled(&reassembly_ctx, reassembled,
                                                           &reassembled_size, sizeof(reassembled));
                    
                    if (get_result == PROTO_SUCCESS && reassembled_size > 0) {
                        char success_log[256];
                        sprintf_s(success_log, sizeof(success_log),
                                 "Reassembly complete! Total size: %zu bytes",
                                 reassembled_size);
                        OutputDebugStringA(success_log);
                        
                        html_response.append((char*)reassembled, reassembled_size);
                    }
                    
                    proto_reset_reassembly(&reassembly_ctx);
                    break; // Done
                } else if (frag_result == PROTO_ERROR_INCOMPLETE) {
                    // Continue to next fragment in buffer
                    continue;
                } else {
                    char err_log[256];
                    sprintf_s(err_log, sizeof(err_log), "Fragment error: %d", frag_result);
                    OutputDebugStringA(err_log);
                    break;
                }
            } else {
                // Non-fragmented response
                uint8_t decrypted[8192];
                size_t decrypted_size = 0;
                
                int decrypt_result = proto_decapsulate(&frag, g_sessionKey, g_session->session_id,
                                                      data_seq, decrypted,
                                                      &decrypted_size, sizeof(decrypted));
                
                if (decrypt_result == PROTO_SUCCESS && decrypted_size > 0) {
                    html_response.append((char*)decrypted, decrypted_size);
                }
                break;
                
                // STREAMING: Check if there are more chunks to fetch
                if (decrypted_size >= 1200) {
                    OutputDebugStringA("First chunk received, fetching remaining chunks...");
                    
                    uint8_t chunk_num = 1;
                    while (true) {
                        // Create request for next chunk (same HTTP request, different fragment_num)
                        proto_packet_t chunk_packets[10];
                        size_t chunk_packet_count;
                        
                        int chunk_result = proto_encapsulate((uint8_t*)http_request.c_str(), http_request.size(),
                                                            g_sessionKey, g_session->session_id, data_seq,
                                                            chunk_packets, &chunk_packet_count, 10);
                        
                        if (chunk_result != PROTO_SUCCESS || chunk_packet_count == 0) break;
                        
                        // Modify fragment_num to request specific chunk
                        chunk_packets[0].header.fragment_num = chunk_num;
                        
                        // Send chunk request
                        uint8_t chunk_send_buffer[2048];
                        memcpy(chunk_send_buffer, &chunk_packets[0].header, sizeof(proto_header_t));
                        memcpy(chunk_send_buffer + sizeof(proto_header_t), chunk_packets[0].payload, chunk_packets[0].payload_size);
                        size_t chunk_send_size = sizeof(proto_header_t) + chunk_packets[0].payload_size;
                        
                        uint8_t chunk_reply_buffer[8192];
                        size_t chunk_reply_size = 0;
                        
                        int chunk_send_result = icmp_send_reliable(g_session, chunk_send_buffer, chunk_send_size,
                                                                   chunk_reply_buffer, &chunk_reply_size);
                        
                        if (chunk_send_result != ICMP_SUCCESS || chunk_reply_size < sizeof(proto_header_t)) {
                            char log[256];
                            sprintf_s(log, sizeof(log), "Chunk %d fetch failed, stopping", chunk_num);
                            OutputDebugStringA(log);
                            break;
                        }
                        
                        // Parse chunk response
                        proto_packet_t chunk_frag;
                        memcpy(&chunk_frag.header, chunk_reply_buffer, sizeof(proto_header_t));
                        chunk_frag.payload_size = chunk_reply_size - sizeof(proto_header_t);
                        memcpy(chunk_frag.payload, chunk_reply_buffer + sizeof(proto_header_t), chunk_frag.payload_size);
                        
                        // Decrypt chunk
                        uint8_t chunk_decrypted[8192];
                        size_t chunk_decrypted_size = 0;
                        
                        int chunk_decrypt_result = proto_decapsulate(&chunk_frag, g_sessionKey, g_session->session_id,
                                                                    data_seq, chunk_decrypted,
                                                                    &chunk_decrypted_size, sizeof(chunk_decrypted));
                        
                        if (chunk_decrypt_result == PROTO_SUCCESS && chunk_decrypted_size > 0) {
                            html_response.append((char*)chunk_decrypted, chunk_decrypted_size);
                            
                            char log[256];
                            sprintf_s(log, sizeof(log), "Received chunk %d: %zu bytes", chunk_num, chunk_decrypted_size);
                            OutputDebugStringA(log);
                            
                            // If chunk is less than 1200 bytes, it's the last one
                            if (chunk_decrypted_size < 1200) {
                                OutputDebugStringA("Last chunk received, streaming complete");
                                break;
                            }
                            
                            chunk_num++;
                        } else {
                            break;
                        }
                    }
                }
            }
        }
    }
    
    
    // Extract HTML from HTTP response
    size_t body_start = html_response.find("\r\n\r\n");
    if (body_start == std::string::npos) {
        body_start = html_response.find("\n\n");
        if (body_start != std::string::npos) {
            body_start += 2;
        }
    } else {
        body_start += 4;
    }
    
    if (body_start != std::string::npos && body_start < html_response.length()) {
        return html_response.substr(body_start);
    }
    
    // If no body separator found, return the whole response
    return html_response;
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

    // Initialize global state
    g_session = NULL;
    g_handshakeComplete = false;
    g_dataSequence = 0;
    memset(g_sessionKey, 0, sizeof(g_sessionKey));

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
