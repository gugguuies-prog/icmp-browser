/**
 * status_bar.cpp
 * 
 * Status Bar Implementation
 */

#include "status_bar.h"
#include <sstream>
#include <iomanip>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

/**
 * Format bytes for display
 */
static std::wstring FormatBytes(uint64_t bytes)
{
    std::wostringstream oss;
    
    if (bytes < 1024) {
        oss << bytes << L" B";
    } else if (bytes < 1024 * 1024) {
        oss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << L" KB";
    } else if (bytes < 1024 * 1024 * 1024) {
        oss << std::fixed << std::setprecision(1) << (bytes / (1024.0 * 1024.0)) << L" MB";
    } else {
        oss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0 * 1024.0)) << L" GB";
    }
    
    return oss.str();
}

/**
 * Constructor
 */
StatusBar::StatusBar(HWND hwndParent)
    : m_hwndParent(hwndParent)
    , m_hWnd(nullptr)
    , m_connected(false)
    , m_latency(0)
    , m_bytesUp(0)
    , m_bytesDown(0)
{
}

/**
 * Destructor
 */
StatusBar::~StatusBar()
{
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
    }
}

/**
 * Create status bar
 */
bool StatusBar::Create()
{
    // Initialize common controls
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);
    
    // Create status bar
    m_hWnd = CreateWindowExW(
        0,
        STATUSCLASSNAMEW,
        nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        m_hwndParent,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    
    if (!m_hWnd) {
        return false;
    }
    
    // Set parts (4 parts: status, connection, latency, traffic)
    int parts[] = { 200, 400, 500, -1 };
    SendMessage(m_hWnd, SB_SETPARTS, 4, (LPARAM)parts);
    
    // Set initial text
    SetStatus(L"Ready");
    
    return true;
}

/**
 * Set status text
 */
void StatusBar::SetStatus(const std::wstring& status)
{
    m_statusText = status;
    
    if (m_hWnd) {
        SendMessageW(m_hWnd, SB_SETTEXTW, 0, (LPARAM)status.c_str());
    }
}

/**
 * Set connection info
 */
void StatusBar::SetConnectionInfo(bool connected, const std::string& serverIp,
                                  int latency, uint64_t bytesUp, uint64_t bytesDown)
{
    m_connected = connected;
    m_serverIp = serverIp;
    m_latency = latency;
    m_bytesUp = bytesUp;
    m_bytesDown = bytesDown;
    
    if (!m_hWnd) {
        return;
    }
    
    // Connection status
    if (connected) {
        std::wstring connText = L"🟢 Connected: ";
        connText += std::wstring(serverIp.begin(), serverIp.end());
        SendMessageW(m_hWnd, SB_SETTEXTW, 1, (LPARAM)connText.c_str());
    } else {
        SendMessageW(m_hWnd, SB_SETTEXTW, 1, (LPARAM)L"🔴 Disconnected");
    }
    
    // Latency
    if (connected && latency > 0) {
        std::wostringstream oss;
        oss << L"⏱ " << latency << L" ms";
        SendMessageW(m_hWnd, SB_SETTEXTW, 2, (LPARAM)oss.str().c_str());
    } else {
        SendMessageW(m_hWnd, SB_SETTEXTW, 2, (LPARAM)L"");
    }
    
    // Traffic
    if (connected) {
        std::wostringstream oss;
        oss << L"↑ " << FormatBytes(bytesUp) << L"  ↓ " << FormatBytes(bytesDown);
        SendMessageW(m_hWnd, SB_SETTEXTW, 3, (LPARAM)oss.str().c_str());
    } else {
        SendMessageW(m_hWnd, SB_SETTEXTW, 3, (LPARAM)L"");
    }
}

/**
 * Update position
 */
void StatusBar::UpdatePosition()
{
    if (m_hWnd) {
        SendMessage(m_hWnd, WM_SIZE, 0, 0);
    }
}
