/**
 * icmp_browser.cpp
 * 
 * ICMP Browser - Modern web browser using ICMP tunnel
 * Simple implementation with native Windows controls
 */

#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <string>
#include <sstream>
#include <thread>

#include "../client/icmp_engine.h"
#include "../common/protocol.h"
#include "../common/handshake.h"
#include "../crypto/aes_gcm.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// Control IDs
#define ID_URL_BAR      1001
#define ID_GO_BUTTON    1002
#define ID_BACK_BUTTON  1003
#define ID_FWD_BUTTON   1004
#define ID_REFRESH_BTN  1005
#define ID_CONTENT      1006
#define ID_STATUS_BAR   1007

// Global variables
HWND g_hwndMain = NULL;
HWND g_hwndUrlBar = NULL;
HWND g_hwndContent = NULL;
HWND g_hwndStatus = NULL;
std::wstring g_currentUrl;
std::string g_serverIp = "192.168.1.40";

// ICMP session
icmp_session_t* g_icmpSession = NULL;
uint8_t g_sessionKey[32] = {0};
bool g_handshakeComplete = false;
uint16_t g_dataSequence = 0;

// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void OnNavigate(HWND hwnd);
void UpdateStatusBar(const std::wstring& text);
std::string FetchUrlViaIcmp(const std::string& url);
void DisplayHtml(const std::string& html);

/**
 * Parse URL to extract host and path
 */
bool ParseUrl(const std::string& url, std::string& host, std::string& path) {
    // Simple URL parser: http://example.com/path
    size_t protoEnd = url.find("://");
    if (protoEnd == std::string::npos) {
        return false;
    }
    
    size_t hostStart = protoEnd + 3;
    size_t pathStart = url.find('/', hostStart);
    
    if (pathStart == std::string::npos) {
        host = url.substr(hostStart);
        path = "/";
    } else {
        host = url.substr(hostStart, pathStart - hostStart);
        path = url.substr(pathStart);
    }
    
    return true;
}

/**
 * Perform handshake with server
 */
bool PerformHandshake() {
    if (g_handshakeComplete) {
        return true;
    }
    
    UpdateStatusBar(L"Établissement de la connexion sécurisée...");
    
    // Create ICMP session
    if (!g_icmpSession) {
        g_icmpSession = icmp_create_session(g_serverIp.c_str());
        if (!g_icmpSession) {
            UpdateStatusBar(L"Erreur: Impossible de créer la session ICMP");
            return false;
        }
    }
    
    // Initialize handshake
    handshake_context_t* hsCtx = handshake_client_init();
    if (!hsCtx) {
        UpdateStatusBar(L"Erreur: Échec initialisation handshake");
        return false;
    }
    
    // Create handshake request
    proto_packet_t request;
    int result = handshake_client_create_request(hsCtx, &request);
    if (result != HANDSHAKE_SUCCESS) {
        handshake_free(hsCtx);
        UpdateStatusBar(L"Erreur: Échec création requête handshake");
        return false;
    }
    
    // Send handshake request
    uint8_t icmpPayload[8 + 256];
    memcpy(icmpPayload, &request.header, 8);
    memcpy(icmpPayload + 8, request.payload, request.payload_size);
    
    uint8_t replyPayload[2048];
    size_t replySize;
    result = icmp_send_reliable(g_icmpSession, icmpPayload, 8 + request.payload_size,
                                 replyPayload, &replySize);
    
    if (result != ICMP_SUCCESS) {
        handshake_free(hsCtx);
        UpdateStatusBar(L"Erreur: Timeout handshake");
        return false;
    }
    
    // Parse response
    proto_packet_t response;
    if (replySize < 8) {
        handshake_free(hsCtx);
        UpdateStatusBar(L"Erreur: Réponse handshake invalide");
        return false;
    }
    
    memcpy(&response.header, replyPayload, 8);
    response.payload_size = replySize - 8;
    memcpy(response.payload, replyPayload + 8, response.payload_size);
    
    // Finalize handshake
    result = handshake_client_finish(hsCtx, &response);
    if (result != HANDSHAKE_SUCCESS) {
        handshake_free(hsCtx);
        UpdateStatusBar(L"Erreur: Échec finalisation handshake");
        return false;
    }
    
    // Get session key
    handshake_get_session_key(hsCtx, g_sessionKey);
    handshake_free(hsCtx);
    
    g_handshakeComplete = true;
    g_dataSequence = 0;
    
    UpdateStatusBar(L"Connexion sécurisée établie");
    return true;
}

/**
 * Create HTTP GET packet (from test_encapsulation_real.c)
 */
extern size_t create_http_get_packet(const char* host, const char* path,
                                      const char* src_ip, const char* dst_ip,
                                      uint8_t* buffer, size_t buffer_size);

/**
 * Fetch URL via ICMP tunnel
 */
std::string FetchUrlViaIcmp(const std::string& url) {
    // Parse URL
    std::string host, path;
    if (!ParseUrl(url, host, path)) {
        return "<html><body><h1>Erreur: URL invalide</h1><p>" + url + "</p></body></html>";
    }
    
    UpdateStatusBar(L"Chargement de " + std::wstring(url.begin(), url.end()) + L"...");
    
    // Perform handshake if needed
    if (!PerformHandshake()) {
        return "<html><body><h1>Erreur de connexion</h1><p>Impossible d'établir la connexion sécurisée</p></body></html>";
    }
    
    // Create HTTP GET packet
    uint8_t ipPacket[2048];
    size_t ipSize = create_http_get_packet(host.c_str(), path.c_str(),
                                            "10.0.1.2", "10.0.1.1",
                                            ipPacket, sizeof(ipPacket));
    
    if (ipSize == 0) {
        return "<html><body><h1>Erreur</h1><p>Impossible de créer le paquet HTTP</p></body></html>";
    }
    
    // Encapsulate
    proto_packet_t packets[10];
    size_t packetCount;
    
    int result = proto_encapsulate(ipPacket, ipSize,
                                    g_sessionKey,
                                    g_icmpSession->session_id,
                                    g_dataSequence,
                                    packets, &packetCount, 10);
    
    if (result != PROTO_SUCCESS) {
        return "<html><body><h1>Erreur d'encapsulation</h1></body></html>";
    }
    
    // Send packet
    for (size_t i = 0; i < packetCount; i++) {
        proto_packet_t* pkt = &packets[i];
        uint8_t icmpPayload[2048];
        memcpy(icmpPayload, &pkt->header, 8);
        memcpy(icmpPayload + 8, pkt->payload, pkt->payload_size);
        size_t totalSize = 8 + pkt->payload_size;
        
        result = icmp_send_packet(g_icmpSession, icmpPayload, totalSize);
        if (result != ICMP_SUCCESS) {
            return "<html><body><h1>Erreur d'envoi</h1></body></html>";
        }
    }
    
    g_dataSequence++;
    
    // Wait for response
    UpdateStatusBar(L"Attente de la réponse...");
    
    uint8_t replyPayload[2048];
    size_t replySize;
    result = icmp_wait_reply(g_icmpSession, replyPayload, &replySize, 10000);
    
    if (result != ICMP_SUCCESS) {
        return "<html><body><h1>Timeout</h1><p>Aucune réponse du serveur</p></body></html>";
    }
    
    // Parse response
    proto_packet_t response;
    memcpy(&response.header, replyPayload, 8);
    response.payload_size = replySize - 8;
    memcpy(response.payload, replyPayload + 8, response.payload_size);
    
    // Decrypt response
    uint8_t decrypted[2048];
    size_t decryptedSize;
    
    result = proto_decapsulate(&response, g_sessionKey,
                                g_icmpSession->session_id,
                                g_dataSequence,
                                decrypted, &decryptedSize, sizeof(decrypted));
    
    if (result != PROTO_SUCCESS) {
        return "<html><body><h1>Erreur de décryptage</h1></body></html>";
    }
    
    g_dataSequence++;
    
    // Extract HTTP response from IP/TCP packet
    // For now, just show raw data
    std::string content((char*)decrypted, decryptedSize);
    
    UpdateStatusBar(L"Chargement terminé");
    
    return "<html><body><h1>Réponse reçue</h1><pre>" + content + "</pre></body></html>";
}

/**
 * Display HTML in content area
 */
void DisplayHtml(const std::string& html) {
    // For now, just display as text in RichEdit control
    // TODO: Implement real HTML rendering
    SetWindowTextA(g_hwndContent, html.c_str());
}

/**
 * Update status bar
 */
void UpdateStatusBar(const std::wstring& text) {
    if (g_hwndStatus) {
        SetWindowTextW(g_hwndStatus, text.c_str());
    }
}

/**
 * Navigate to URL (in background thread)
 */
void OnNavigate(HWND hwnd) {
    wchar_t urlBuffer[1024];
    GetWindowTextW(g_hwndUrlBar, urlBuffer, 1024);
    
    std::wstring wurl(urlBuffer);
    if (wurl.empty()) {
        return;
    }
    
    g_currentUrl = wurl;
    
    // Convert to UTF-8
    int size = WideCharToMultiByte(CP_UTF8, 0, wurl.c_str(), -1, NULL, 0, NULL, NULL);
    std::string url(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wurl.c_str(), -1, &url[0], size, NULL, NULL);
    url.resize(size - 1); // Remove null terminator
    
    // Fetch in background thread
    std::thread([url]() {
        std::string html = FetchUrlViaIcmp(url);
        
        // Update UI in main thread
        PostMessage(g_hwndMain, WM_USER + 1, 0, (LPARAM)new std::string(html));
    }).detach();
}

/**
 * Create all controls
 */
void CreateControls(HWND hwnd) {
    HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    
    // Toolbar area (60px height)
    int toolbarY = 0;
    int toolbarHeight = 60;
    int buttonSize = 40;
    int margin = 10;
    int x = margin;
    
    // Back button
    g_hwndContent = CreateWindowExW(0, L"BUTTON", L"←",
                                     WS_CHILD | WS_VISIBLE | BS_FLAT,
                                     x, margin, buttonSize, buttonSize,
                                     hwnd, (HMENU)ID_BACK_BUTTON, NULL, NULL);
    x += buttonSize + 5;
    
    // Forward button
    CreateWindowExW(0, L"BUTTON", L"→",
                    WS_CHILD | WS_VISIBLE | BS_FLAT,
                    x, margin, buttonSize, buttonSize,
                    hwnd, (HMENU)ID_FWD_BUTTON, NULL, NULL);
    x += buttonSize + 5;
    
    // Refresh button
    CreateWindowExW(0, L"BUTTON", L"⟳",
                    WS_CHILD | WS_VISIBLE | BS_FLAT,
                    x, margin, buttonSize, buttonSize,
                    hwnd, (HMENU)ID_REFRESH_BTN, NULL, NULL);
    x += buttonSize + 10;
    
    // URL bar
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int urlBarWidth = clientRect.right - x - buttonSize - margin * 2 - 5;
    
    g_hwndUrlBar = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"http://example.com",
                                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                    x, margin, urlBarWidth, buttonSize,
                                    hwnd, (HMENU)ID_URL_BAR, NULL, NULL);
    SendMessage(g_hwndUrlBar, WM_SETFONT, (WPARAM)hFont, TRUE);
    x += urlBarWidth + 5;
    
    // Go button
    CreateWindowExW(0, L"BUTTON", L"→",
                    WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                    x, margin, buttonSize, buttonSize,
                    hwnd, (HMENU)ID_GO_BUTTON, NULL, NULL);
    
    // Content area (RichEdit for now)
    LoadLibrary(L"Msftedit.dll");
    g_hwndContent = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
                                     WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                                     margin, toolbarHeight + margin, 
                                     clientRect.right - margin * 2,
                                     clientRect.bottom - toolbarHeight - margin * 2 - 30,
                                     hwnd, (HMENU)ID_CONTENT, NULL, NULL);
    SendMessage(g_hwndContent, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Status bar
    g_hwndStatus = CreateWindowExW(0, L"STATIC", L"Prêt",
                                    WS_CHILD | WS_VISIBLE | SS_LEFT,
                                    margin, clientRect.bottom - 25,
                                    clientRect.right - margin * 2, 20,
                                    hwnd, (HMENU)ID_STATUS_BAR, NULL, NULL);
    SendMessage(g_hwndStatus, WM_SETFONT, (WPARAM)hFont, TRUE);
}

/**
 * Window procedure
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            CreateControls(hwnd);
            return 0;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_GO_BUTTON || 
                (LOWORD(wParam) == ID_URL_BAR && HIWORD(wParam) == EN_SETFOCUS)) {
                if (LOWORD(wParam) == ID_GO_BUTTON) {
                    OnNavigate(hwnd);
                }
            }
            return 0;
            
        case WM_SIZE: {
            // Resize controls
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            
            int margin = 10;
            int toolbarHeight = 60;
            int buttonSize = 40;
            
            // Resize URL bar
            int urlBarX = margin + buttonSize * 3 + 20;
            int urlBarWidth = clientRect.right - urlBarX - buttonSize - margin * 2 - 5;
            SetWindowPos(g_hwndUrlBar, NULL, urlBarX, margin, urlBarWidth, buttonSize, SWP_NOZORDER);
            
            // Resize content area
            SetWindowPos(g_hwndContent, NULL, margin, toolbarHeight + margin,
                        clientRect.right - margin * 2,
                        clientRect.bottom - toolbarHeight - margin * 2 - 30,
                        SWP_NOZORDER);
            
            // Resize status bar
            SetWindowPos(g_hwndStatus, NULL, margin, clientRect.bottom - 25,
                        clientRect.right - margin * 2, 20, SWP_NOZORDER);
            
            return 0;
        }
        
        case WM_USER + 1: {
            // Display HTML (called from background thread)
            std::string* html = (std::string*)lParam;
            DisplayHtml(*html);
            delete html;
            return 0;
        }
        
        case WM_DESTROY:
            if (g_icmpSession) {
                icmp_destroy_session(g_icmpSession);
            }
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/**
 * WinMain entry point
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);
    
    // Register window class
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ICMPBrowserWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    
    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"Échec de l'enregistrement de la classe de fenêtre", L"Erreur", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Create main window
    g_hwndMain = CreateWindowExW(0, L"ICMPBrowserWindow", L"ICMP Browser",
                                  WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
                                  NULL, NULL, hInstance, NULL);
    
    if (!g_hwndMain) {
        MessageBoxW(NULL, L"Échec de la création de la fenêtre", L"Erreur", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    ShowWindow(g_hwndMain, nCmdShow);
    UpdateWindow(g_hwndMain);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}
