/**
 * icmp_browser_simple.cpp
 * 
 * Simple ICMP Browser with modern Windows UI
 * Reuses the tested ICMP tunnel code
 */

// IMPORTANT: Include winsock2.h BEFORE windows.h to avoid conflicts
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <string>
#include <vector>
#include <thread>
#include <sstream>
#include <cstdio>

#include "html_renderer.h"

// Include C headers
#include "../client/icmp_engine.h"
#include "../common/protocol.h"
#include "../common/handshake.h"
#include "../crypto/aes_gcm.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Control IDs
#define ID_URL_BAR      1001
#define ID_GO_BUTTON    1002
#define ID_BACK_BUTTON  1003
#define ID_FWD_BUTTON   1004
#define ID_REFRESH_BTN  1005
#define ID_CONTENT      1006
#define ID_STATUS       1007
#define ID_SETTINGS_BTN 1008
#define ID_MENU_SERVER  2001
#define ID_MENU_HISTORY 2002
#define ID_MENU_BOOKMARKS 2003
#define ID_MENU_ABOUT   2004

// Global state
HWND g_hwndMain = NULL;
HWND g_hwndUrlBar = NULL;
HWND g_hwndContent = NULL;
HWND g_hwndStatus = NULL;
HWND g_hwndGoBtn = NULL;
HWND g_hwndBackBtn = NULL;
HWND g_hwndFwdBtn = NULL;
HWND g_hwndRefreshBtn = NULL;

icmp_session_t* g_session = NULL;
uint8_t g_sessionKey[32];
bool g_handshakeComplete = false;
uint16_t g_dataSequence = 0;
std::string g_serverIp = "192.168.1.40";
std::vector<std::string> g_history;
int g_historyPos = -1;

// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateUI(HWND hwnd);
void OnNavigate();
void OnBack();
void OnForward();
void OnRefresh();
void ShowServerDialog();
void UpdateStatus(const std::wstring& text);
void SetContent(const std::string& text);
void EnableNavigationButtons();

// Copy from test file
extern "C" {
    uint16_t calculate_checksum(uint16_t *addr, int len);
    size_t create_http_get_packet(const char *host, const char *path,
                                   const char *src_ip, const char *dst_ip,
                                   uint8_t *buffer, size_t max_size);
}

// Checksum calculation
uint16_t calculate_checksum(uint16_t *addr, int len) {
    int nleft = len;
    uint32_t sum = 0;
    uint16_t *w = addr;
    uint16_t answer = 0;

    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1) {
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;

    return answer;
}

// Create HTTP GET packet
size_t create_http_get_packet(const char *host, const char *path,
                               const char *src_ip, const char *dst_ip,
                               uint8_t *buffer, size_t max_size) {
#pragma pack(push, 1)
    struct iphdr {
        uint8_t version_ihl;
        uint8_t tos;
        uint16_t tot_len;
        uint16_t id;
        uint16_t frag_off;
        uint8_t ttl;
        uint8_t protocol;
        uint16_t check;
        uint32_t saddr;
        uint32_t daddr;
    };

    struct tcphdr {
        uint16_t source;
        uint16_t dest;
        uint32_t seq;
        uint32_t ack_seq;
        uint16_t res1_doff_flags;
        uint16_t window;
        uint16_t check;
        uint16_t urg_ptr;
    };
#pragma pack(pop)

    char http_request[1024];
    snprintf(http_request, sizeof(http_request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: ICMP-Browser/1.0\r\n"
             "Accept: text/html\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, host);

    size_t http_len = strlen(http_request);
    size_t total_len = sizeof(struct iphdr) + sizeof(struct tcphdr) + http_len;

    if (total_len > max_size) return 0;

    struct iphdr *ip = (struct iphdr *)buffer;
    memset(ip, 0, sizeof(struct iphdr));
    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->tot_len = htons((uint16_t)total_len);
    ip->id = htons(rand() % 65535);
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->protocol = 6;
    ip->check = 0;
    ip->saddr = inet_addr(src_ip);
    ip->daddr = inet_addr(dst_ip);
    ip->check = calculate_checksum((uint16_t *)ip, sizeof(struct iphdr));

    struct tcphdr *tcp = (struct tcphdr *)(buffer + sizeof(struct iphdr));
    memset(tcp, 0, sizeof(struct tcphdr));
    tcp->source = htons(12345);
    tcp->dest = htons(80);
    tcp->seq = htonl(rand());
    tcp->ack_seq = 0;
    tcp->res1_doff_flags = htons(0x5002);
    tcp->window = htons(65535);
    tcp->check = 0;
    tcp->urg_ptr = 0;

    memcpy(buffer + sizeof(struct iphdr) + sizeof(struct tcphdr), 
           http_request, http_len);

    tcp->check = calculate_checksum((uint16_t *)tcp, 
                                    sizeof(struct tcphdr) + (int)http_len);

    return total_len;
}

// Parse URL
bool ParseUrl(const std::string& url, std::string& host, std::string& path) {
    size_t protoEnd = url.find("://");
    if (protoEnd == std::string::npos) return false;
    
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

// Perform handshake
bool DoHandshake() {
    if (g_handshakeComplete) return true;
    
    UpdateStatus(L"Etablissement de la connexion securisee...");
    
    if (!g_session) {
        g_session = icmp_session_create(g_serverIp.c_str());
        if (!g_session) {
            UpdateStatus(L"[X] Impossible de creer la session ICMP");
            return false;
        }
    }
    
    handshake_context_t hsCtx;
    if (handshake_init(&hsCtx, g_session->session_id) != HANDSHAKE_SUCCESS) {
        UpdateStatus(L"[X] Echec initialisation handshake");
        return false;
    }
    
    proto_packet_t request;
    if (handshake_client_start(&hsCtx, &request) != HANDSHAKE_SUCCESS) {
        handshake_cleanup(&hsCtx);
        UpdateStatus(L"[X] Echec creation requete handshake");
        return false;
    }
    
    uint8_t icmpPayload[8 + 256];
    memcpy(icmpPayload, &request.header, 8);
    memcpy(icmpPayload + 8, request.payload, request.payload_size);
    
    uint8_t replyPayload[2048];
    size_t replySize;
    if (icmp_send_reliable(g_session, icmpPayload, 8 + request.payload_size,
                           replyPayload, &replySize) != ICMP_SUCCESS) {
        handshake_cleanup(&hsCtx);
        UpdateStatus(L"[X] Timeout handshake");
        return false;
    }
    
    proto_packet_t response;
    if (replySize < 8) {
        handshake_cleanup(&hsCtx);
        UpdateStatus(L"[X] Reponse handshake invalide");
        return false;
    }
    
    memcpy(&response.header, replyPayload, 8);
    response.payload_size = replySize - 8;
    memcpy(response.payload, replyPayload + 8, response.payload_size);
    
    if (handshake_client_finish(&hsCtx, &response) != HANDSHAKE_SUCCESS) {
        handshake_cleanup(&hsCtx);
        UpdateStatus(L"[X] Echec finalisation handshake");
        return false;
    }
    
    handshake_get_session_key(&hsCtx, g_sessionKey);
    handshake_cleanup(&hsCtx);
    
    g_handshakeComplete = true;
    g_dataSequence = 0;
    
    UpdateStatus(L"[OK] Connexion securisee etablie");
    return true;
}

// Fetch URL
std::string FetchUrl(const std::string& url) {
    std::string host, path;
    if (!ParseUrl(url, host, path)) {
        return "Erreur: URL invalide\n" + url;
    }
    
    UpdateStatus((L"Chargement de " + std::wstring(url.begin(), url.end()) + L"...").c_str());
    
    if (!DoHandshake()) {
        return "Erreur de connexion: Impossible d'établir la connexion sécurisée avec le serveur ICMP";
    }
    
    // Create HTTP packet
    uint8_t ipPacket[2048];
    size_t ipSize = create_http_get_packet(host.c_str(), path.c_str(),
                                            "10.0.1.2", "10.0.1.1",
                                            ipPacket, sizeof(ipPacket));
    
    if (ipSize == 0) {
        return "Erreur: Impossible de créer le paquet HTTP";
    }
    
    // Encapsulate
    proto_packet_t packets[10];
    size_t packetCount;
    
    if (proto_encapsulate(ipPacket, ipSize, g_sessionKey,
                          g_session->session_id, g_dataSequence,
                          packets, &packetCount, 10) != PROTO_SUCCESS) {
        return "Erreur d'encapsulation";
    }
    
    // Send
    for (size_t i = 0; i < packetCount; i++) {
        proto_packet_t* pkt = &packets[i];
        uint8_t icmpPayload[2048];
        memcpy(icmpPayload, &pkt->header, 8);
        memcpy(icmpPayload + 8, pkt->payload, pkt->payload_size);
        size_t totalSize = 8 + pkt->payload_size;
        
        if (icmp_send_packet(g_session, icmpPayload, totalSize) != ICMP_SUCCESS) {
            return "Erreur d'envoi";
        }
    }
    
    g_dataSequence++;
    
    // Wait for response
    uint8_t replyPayload[2048];
    size_t replySize;
    if (icmp_wait_reply(g_session, replyPayload, &replySize, 10000) != ICMP_SUCCESS) {
        return "Timeout: Aucune réponse du serveur";
    }
    
    // Decrypt
    proto_packet_t response;
    memcpy(&response.header, replyPayload, 8);
    response.payload_size = replySize - 8;
    memcpy(response.payload, replyPayload + 8, response.payload_size);
    
    uint8_t decrypted[2048];
    size_t decryptedSize;
    
    if (proto_decapsulate(&response, g_sessionKey, g_session->session_id,
                          g_dataSequence, decrypted, &decryptedSize,
                          sizeof(decrypted)) != PROTO_SUCCESS) {
        return "Erreur de décryptage";
    }
    
    g_dataSequence++;
    
    UpdateStatus(L"[OK] Chargement termine");
    
    // Extract HTTP response from IP/TCP packet
    // IP header: 20 bytes minimum
    // TCP header: 20 bytes minimum
    if (decryptedSize < 40) {
        return "Erreur: Paquet trop petit";
    }
    
    // Skip IP header (check IHL field for actual size)
    uint8_t ihl = (decrypted[0] & 0x0F) * 4; // IHL is in 32-bit words
    if (ihl < 20 || ihl > 60) ihl = 20;
    
    // Skip TCP header (check data offset for actual size)
    if (decryptedSize < ihl + 20) {
        return "Erreur: Paquet TCP invalide";
    }
    
    uint8_t tcpDataOffset = ((decrypted[ihl + 12] >> 4) & 0x0F) * 4;
    if (tcpDataOffset < 20) tcpDataOffset = 20;
    
    size_t httpStart = ihl + tcpDataOffset;
    if (httpStart >= decryptedSize) {
        return "Erreur: Pas de données HTTP";
    }
    
    // Extract HTTP response
    std::string httpResponse((char*)decrypted + httpStart, decryptedSize - httpStart);
    
    // Parse HTTP response - find the blank line that separates headers from body
    size_t bodyStart = httpResponse.find("\r\n\r\n");
    if (bodyStart == std::string::npos) {
        bodyStart = httpResponse.find("\n\n");
        if (bodyStart != std::string::npos) {
            bodyStart += 2;
        }
    } else {
        bodyStart += 4;
    }
    
    if (bodyStart == std::string::npos || bodyStart >= httpResponse.length()) {
        // No body found, return the whole response
        return httpResponse;
    }
    
    // Extract only the HTML body
    std::string htmlBody = httpResponse.substr(bodyStart);
    
    // If empty or very small, show the full response for debugging
    if (htmlBody.length() < 10) {
        return "Réponse HTTP complète:\n\n" + httpResponse;
    }
    
    return htmlBody;
}

// Update status bar
void UpdateStatus(const std::wstring& text) {
    if (g_hwndStatus) {
        // Replace special characters that might not render well
        std::wstring cleanText = text;
        // Replace • with -
        for (size_t i = 0; i < cleanText.length(); i++) {
            if (cleanText[i] > 127) {
                cleanText[i] = L'-';
            }
        }
        SetWindowTextW(g_hwndStatus, cleanText.c_str());
    }
}

// Set content
void SetContent(const std::string& text) {
    if (g_hwndContent) {
        // Check if it looks like HTML
        if (text.find("<html") != std::string::npos || 
            text.find("<HTML") != std::string::npos ||
            text.find("<!DOCTYPE") != std::string::npos ||
            text.find("<head") != std::string::npos ||
            text.find("<body") != std::string::npos) {
            
            // Use HTML renderer
            HtmlRenderer renderer;
            auto elements = renderer.Render(text);
            renderer.ApplyToRichEdit(g_hwndContent, elements);
            
            // Update window title with page title
            std::string title = HtmlRenderer::ExtractTitle(text);
            std::wstring wtitle = L"ICMP Browser - " + std::wstring(title.begin(), title.end());
            SetWindowTextW(g_hwndMain, wtitle.c_str());
        } else {
            // Plain text
            SetWindowTextA(g_hwndContent, text.c_str());
        }
    }
}

// Navigate (background thread)
void OnNavigate() {
    wchar_t urlBuffer[1024];
    GetWindowTextW(g_hwndUrlBar, urlBuffer, 1024);
    
    std::wstring wurl(urlBuffer);
    if (wurl.empty()) return;
    
    // Convert to UTF-8
    int size = WideCharToMultiByte(CP_UTF8, 0, wurl.c_str(), -1, NULL, 0, NULL, NULL);
    std::string url(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wurl.c_str(), -1, &url[0], size, NULL, NULL);
    url.resize(size - 1);
    
    // Add to history
    if (g_historyPos == -1 || g_history[g_historyPos] != url) {
        // Remove forward history
        if (g_historyPos < (int)g_history.size() - 1) {
            g_history.erase(g_history.begin() + g_historyPos + 1, g_history.end());
        }
        g_history.push_back(url);
        g_historyPos = g_history.size() - 1;
    }
    
    // Disable navigation buttons
    EnableWindow(g_hwndGoBtn, FALSE);
    EnableWindow(g_hwndBackBtn, FALSE);
    EnableWindow(g_hwndFwdBtn, FALSE);
    EnableWindow(g_hwndRefreshBtn, FALSE);
    
    // Fetch in background
    std::thread([url]() {
        std::string content = FetchUrl(url);
        PostMessage(g_hwndMain, WM_USER + 1, 0, (LPARAM)new std::string(content));
        PostMessage(g_hwndMain, WM_USER + 2, 0, 0); // Re-enable buttons
    }).detach();
}

// Go back
void OnBack() {
    if (g_historyPos > 0) {
        g_historyPos--;
        std::wstring wurl(g_history[g_historyPos].begin(), g_history[g_historyPos].end());
        SetWindowTextW(g_hwndUrlBar, wurl.c_str());
        OnNavigate();
    }
}

// Go forward
void OnForward() {
    if (g_historyPos < (int)g_history.size() - 1) {
        g_historyPos++;
        std::wstring wurl(g_history[g_historyPos].begin(), g_history[g_historyPos].end());
        SetWindowTextW(g_hwndUrlBar, wurl.c_str());
        OnNavigate();
    }
}

// Refresh
void OnRefresh() {
    OnNavigate();
}

// Show server configuration dialog
void ShowServerDialog() {
    char serverIp[256];
    strcpy_s(serverIp, g_serverIp.c_str());
    
    // Simple input box (you can enhance this with a proper dialog)
    std::wstring prompt = L"Entrez l'adresse IP du serveur ICMP:\n\nActuel: " + 
                          std::wstring(g_serverIp.begin(), g_serverIp.end());
    
    wchar_t buffer[256];
    wcscpy_s(buffer, std::wstring(g_serverIp.begin(), g_serverIp.end()).c_str());
    
    // For now, just show a message box with current server
    std::wstring msg = L"Serveur ICMP actuel:\n\n" + 
                       std::wstring(g_serverIp.begin(), g_serverIp.end()) + 
                       L"\n\nPour changer le serveur, redémarrez le navigateur avec:\n" +
                       L"icmp_browser_simple.exe <server_ip>";
    
    MessageBoxW(g_hwndMain, msg.c_str(), L"Configuration du serveur", MB_OK | MB_ICONINFORMATION);
}

// Enable/disable navigation buttons based on history
void EnableNavigationButtons() {
    EnableWindow(g_hwndBackBtn, g_historyPos > 0);
    EnableWindow(g_hwndFwdBtn, g_historyPos < (int)g_history.size() - 1);
    EnableWindow(g_hwndRefreshBtn, g_historyPos >= 0);
    EnableWindow(g_hwndGoBtn, TRUE);
}

// Create UI
void CreateUI(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    
    HFONT hFont = CreateFontA(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    
    int margin = 10;
    int toolbarHeight = 50;
    int btnSize = 35;
    int y = margin;
    int x = margin;
    
    // Back button - simple <
    g_hwndBackBtn = CreateWindowExW(0, L"BUTTON", L"<",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     x, y, btnSize, btnSize,
                                     hwnd, (HMENU)ID_BACK_BUTTON, NULL, NULL);
    SendMessage(g_hwndBackBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    x += btnSize + 5;
    
    // Forward button - simple >
    g_hwndFwdBtn = CreateWindowExW(0, L"BUTTON", L">",
                                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    x, y, btnSize, btnSize,
                                    hwnd, (HMENU)ID_FWD_BUTTON, NULL, NULL);
    SendMessage(g_hwndFwdBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    x += btnSize + 5;
    
    // Refresh button - ↻
    g_hwndRefreshBtn = CreateWindowExW(0, L"BUTTON", L"↻",
                                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        x, y, btnSize, btnSize,
                                        hwnd, (HMENU)ID_REFRESH_BTN, NULL, NULL);
    SendMessage(g_hwndRefreshBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    x += btnSize + 10;
    
    // Calculate remaining space properly
    int settingsBtnWidth = btnSize;
    int goBtnWidth = 50; // Make Go button wider
    int urlBarWidth = rc.right - x - goBtnWidth - settingsBtnWidth - margin * 2 - 15;
    
    // URL bar
    g_hwndUrlBar = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"http://example.com",
                                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                    x, y, urlBarWidth, btnSize,
                                    hwnd, (HMENU)ID_URL_BAR, NULL, NULL);
    SendMessage(g_hwndUrlBar, WM_SETFONT, (WPARAM)hFont, TRUE);
    x += urlBarWidth + 5;
    
    // Go button - wider with text "GO"
    g_hwndGoBtn = CreateWindowExW(0, L"BUTTON", L"GO",
                                   WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                   x, y, goBtnWidth, btnSize,
                                   hwnd, (HMENU)ID_GO_BUTTON, NULL, NULL);
    SendMessage(g_hwndGoBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    x += goBtnWidth + 5;
    
    // Settings button - ⚙ or just "..."
    HWND hSettingsBtn = CreateWindowExW(0, L"BUTTON", L"⋮",
                                         WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                         x, y, settingsBtnWidth, btnSize,
                                         hwnd, (HMENU)ID_SETTINGS_BTN, NULL, NULL);
    SendMessage(hSettingsBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Content area
    LoadLibraryW(L"Msftedit.dll");
    g_hwndContent = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
                                     WS_CHILD | WS_VISIBLE | WS_VSCROLL | 
                                     ES_MULTILINE | ES_READONLY,
                                     margin, toolbarHeight + margin,
                                     rc.right - margin * 2,
                                     rc.bottom - toolbarHeight - margin * 2 - 30,
                                     hwnd, (HMENU)ID_CONTENT, NULL, NULL);
    SendMessage(g_hwndContent, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Status bar - fix encoding
    std::wstring statusText = L"Pret • Serveur: ";
    for (char c : g_serverIp) {
        statusText += (wchar_t)c;
    }
    
    g_hwndStatus = CreateWindowExW(0, L"STATIC", statusText.c_str(),
                                    WS_CHILD | WS_VISIBLE | SS_LEFT,
                                    margin, rc.bottom - 25,
                                    rc.right - margin * 2, 20,
                                    hwnd, (HMENU)ID_STATUS, NULL, NULL);
    SendMessage(g_hwndStatus, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Enable/disable navigation buttons
    EnableNavigationButtons();
}

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            CreateUI(hwnd);
            return 0;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_GO_BUTTON) {
                OnNavigate();
            } else if (LOWORD(wParam) == ID_BACK_BUTTON) {
                OnBack();
            } else if (LOWORD(wParam) == ID_FWD_BUTTON) {
                OnForward();
            } else if (LOWORD(wParam) == ID_REFRESH_BTN) {
                OnRefresh();
            } else if (LOWORD(wParam) == ID_SETTINGS_BTN) {
                ShowServerDialog();
            }
            return 0;
            
        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            int margin = 10;
            int toolbarHeight = 50;
            int btnSize = 35;
            
            // Resize URL bar
            int urlX = margin + btnSize * 3 + 20;
            int urlWidth = rc.right - urlX - btnSize - margin * 2 - 5;
            SetWindowPos(g_hwndUrlBar, NULL, urlX, margin, urlWidth, btnSize, SWP_NOZORDER);
            
            // Resize content
            SetWindowPos(g_hwndContent, NULL, margin, toolbarHeight + margin,
                        rc.right - margin * 2,
                        rc.bottom - toolbarHeight - margin * 2 - 30,
                        SWP_NOZORDER);
            
            // Resize status
            SetWindowPos(g_hwndStatus, NULL, margin, rc.bottom - 25,
                        rc.right - margin * 2, 20, SWP_NOZORDER);
            return 0;
        }
        
        case WM_USER + 1: {
            std::string* content = (std::string*)lParam;
            SetContent(*content);
            delete content;
            return 0;
        }
        
        case WM_USER + 2:
            EnableNavigationButtons();
            return 0;
            
        case WM_DESTROY:
            if (g_session) {
                icmp_session_destroy(g_session);
            }
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    // Parse command line for server IP
    if (lpCmdLine && strlen(lpCmdLine) > 0) {
        g_serverIp = lpCmdLine;
        // Remove quotes if present
        if (g_serverIp.front() == '"') g_serverIp.erase(0, 1);
        if (g_serverIp.back() == '"') g_serverIp.pop_back();
    }
    
    // Init common controls
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    
    // Register window class
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ICMPBrowserWindow";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"Échec enregistrement classe fenêtre", L"Erreur", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Create window
    g_hwndMain = CreateWindowExW(0, L"ICMPBrowserWindow", L"ICMP Browser",
                                  WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
                                  NULL, NULL, hInstance, NULL);
    
    if (!g_hwndMain) {
        MessageBoxW(NULL, L"Échec création fenêtre", L"Erreur", MB_OK | MB_ICONERROR);
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
