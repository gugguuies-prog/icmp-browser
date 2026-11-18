/**
 * main_window.cpp
 * 
 * Main Window Implementation
 */

#include "main_window.h"
#include "webview_controller.h"
#include "status_bar.h"
#include "settings_dialog.h"
#include "../icmp_engine.h"      // Include BEFORE http_proxy.h (defines icmp_session_t)
#include "../http_proxy.h"
#include <string>
#include <sstream>
#include <commctrl.h>
#include <wininet.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "wininet.lib")

// Window class name
static const wchar_t* WINDOW_CLASS_NAME = L"ICMPBrowserWindow";

// Control IDs
#define IDC_ADDRESS_BAR 1001
#define IDC_GO_BUTTON 1002
#define IDC_BACK_BUTTON 1003
#define IDC_FORWARD_BUTTON 1004
#define IDC_REFRESH_BUTTON 1005
#define IDC_HOME_BUTTON 1006
#define IDC_SETTINGS_BUTTON 1007
#define IDC_MENU_SETTINGS 2001
#define IDC_MENU_EXIT 2002
#define IDC_MENU_ABOUT 2003
#define TIMER_ID_STATUS_UPDATE 100

/**
 * Constructor
 */
MainWindow::MainWindow()
    : m_hWnd(nullptr)
    , m_hAddressBar(nullptr)
    , m_hGoButton(nullptr)
    , m_hBackButton(nullptr)
    , m_hForwardButton(nullptr)
    , m_hRefreshButton(nullptr)
    , m_hHomeButton(nullptr)
    , m_hSettingsButton(nullptr)
    , m_statusBar(nullptr)
    , m_webViewController(nullptr)
    , m_httpProxy(nullptr)
    , m_icmpSession(nullptr)
    , m_tunnelActive(false)
    , m_timerID(0)
{
    // Initialize default settings
    m_settings.serverIp = "192.168.1.100";
    m_settings.proxyPort = 8888;
    m_settings.timeout = 5000;
    m_settings.autoConnect = false;
    m_settings.compression = true;
    m_settings.statistics = true;
}

/**
 * Destructor
 */
MainWindow::~MainWindow()
{
    // Stop timer
    if (m_timerID != 0) {
        KillTimer(m_hWnd, m_timerID);
    }
    
    StopTunnel();
    
    if (m_statusBar) {
        delete m_statusBar;
    }
    
    if (m_webViewController) {
        delete m_webViewController;
    }
}

/**
 * Create main window
 */
bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow)
{
    // Initialize common controls
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_BAR_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);
    
    // Register window class
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WindowProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = WINDOW_CLASS_NAME;
    
    if (!RegisterClassExW(&wcex)) {
        MessageBoxW(nullptr, L"Failed to register window class", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    // Create window
    m_hWnd = CreateWindowExW(
        0,
        WINDOW_CLASS_NAME,
        L"ICMP Browser",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 800,
        nullptr,
        nullptr,
        hInstance,
        this  // Pass this pointer for use in WindowProc
    );
    
    if (!m_hWnd) {
        MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    // Create controls
    CreateControls();
    
    // Initialize WebView2
    InitializeWebView();
    
    // Show window
    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);
    
    // Start status update timer (every 1 second)
    m_timerID = SetTimer(m_hWnd, TIMER_ID_STATUS_UPDATE, 1000, nullptr);
    
    UpdateStatus(L"Ready. Please configure server address.");
    
    return true;
}

/**
 * Run message loop
 */
int MainWindow::Run()
{
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}

/**
 * Create UI controls
 */
void MainWindow::CreateControls()
{
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    int x = BUTTON_MARGIN;
    int y = BUTTON_MARGIN;
    
    // Back button
    m_hBackButton = CreateWindowW(
        L"BUTTON", L"←",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, BUTTON_WIDTH, BUTTON_HEIGHT,
        m_hWnd, (HMENU)IDC_BACK_BUTTON, hInstance, nullptr
    );
    x += BUTTON_WIDTH + BUTTON_MARGIN;
    
    // Forward button
    m_hForwardButton = CreateWindowW(
        L"BUTTON", L"→",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, BUTTON_WIDTH, BUTTON_HEIGHT,
        m_hWnd, (HMENU)IDC_FORWARD_BUTTON, hInstance, nullptr
    );
    x += BUTTON_WIDTH + BUTTON_MARGIN;
    
    // Refresh button
    m_hRefreshButton = CreateWindowW(
        L"BUTTON", L"⟳",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, BUTTON_WIDTH, BUTTON_HEIGHT,
        m_hWnd, (HMENU)IDC_REFRESH_BUTTON, hInstance, nullptr
    );
    x += BUTTON_WIDTH + BUTTON_MARGIN;
    
    // Home button
    m_hHomeButton = CreateWindowW(
        L"BUTTON", L"⌂",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, BUTTON_WIDTH, BUTTON_HEIGHT,
        m_hWnd, (HMENU)IDC_HOME_BUTTON, hInstance, nullptr
    );
    x += BUTTON_WIDTH + BUTTON_MARGIN * 2;
    
    // Get client rect for address bar width
    RECT rcClient;
    GetClientRect(m_hWnd, &rcClient);
    int addressBarWidth = rcClient.right - x - BUTTON_WIDTH * 2 - BUTTON_MARGIN * 3;
    
    // Address bar
    m_hAddressBar = CreateWindowW(
        L"EDIT", L"https://www.google.com",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        x, y, addressBarWidth, BUTTON_HEIGHT,
        m_hWnd, (HMENU)IDC_ADDRESS_BAR, hInstance, nullptr
    );
    x += addressBarWidth + BUTTON_MARGIN;
    
    // Go button
    m_hGoButton = CreateWindowW(
        L"BUTTON", L"Go",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, BUTTON_WIDTH, BUTTON_HEIGHT,
        m_hWnd, (HMENU)IDC_GO_BUTTON, hInstance, nullptr
    );
    x += BUTTON_WIDTH + BUTTON_MARGIN;
    
    // Settings button
    m_hSettingsButton = CreateWindowW(
        L"BUTTON", L"⚙",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, BUTTON_WIDTH, BUTTON_HEIGHT,
        m_hWnd, (HMENU)IDC_SETTINGS_BUTTON, hInstance, nullptr
    );
    
    // Create enhanced status bar
    m_statusBar = new StatusBar(m_hWnd);
    m_statusBar->Create();
}

/**
 * Initialize WebView2
 */
void MainWindow::InitializeWebView()
{
    m_webViewController = new WebViewController(m_hWnd);
    
    // Initialize with callback
    m_webViewController->Initialize([this]() {
        UpdateStatus(L"WebView2 initialized. Ready to browse.");
        
        // Set proxy configuration
        // Note: This will be configured to use 127.0.0.1:8888
        Navigate(L"https://www.google.com");
    });
}

/**
 * Window procedure
 */
LRESULT CALLBACK MainWindow::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    MainWindow* window = nullptr;
    
    if (message == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)window);
    } else {
        window = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }
    
    if (window) {
        switch (message) {
            case WM_SIZE:
                window->OnResize();
                return 0;
                
            case WM_TIMER:
                if (wParam == TIMER_ID_STATUS_UPDATE) {
                    window->OnTimer();
                    return 0;
                }
                break;
                
            case WM_COMMAND:
                switch (LOWORD(wParam)) {
                    case IDC_GO_BUTTON:
                        window->OnNavigate();
                        return 0;
                    case IDC_BACK_BUTTON:
                        window->OnBack();
                        return 0;
                    case IDC_FORWARD_BUTTON:
                        window->OnForward();
                        return 0;
                    case IDC_REFRESH_BUTTON:
                        window->OnRefresh();
                        return 0;
                    case IDC_HOME_BUTTON:
                        window->OnHome();
                        return 0;
                    case IDC_SETTINGS_BUTTON:
                        window->OnSettings();
                        return 0;
                }
                break;
                
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
    }
    
    return DefWindowProc(hWnd, message, wParam, lParam);
}

/**
 * Handle resize
 */
void MainWindow::OnResize()
{
    RECT rcClient;
    GetClientRect(m_hWnd, &rcClient);
    
    // Resize address bar and buttons
    int x = BUTTON_WIDTH * 4 + BUTTON_MARGIN * 6;
    int addressBarWidth = rcClient.right - x - BUTTON_WIDTH * 2 - BUTTON_MARGIN * 3;
    
    SetWindowPos(m_hAddressBar, nullptr, x, BUTTON_MARGIN, 
                 addressBarWidth, BUTTON_HEIGHT, SWP_NOZORDER);
    SetWindowPos(m_hGoButton, nullptr, x + addressBarWidth + BUTTON_MARGIN, BUTTON_MARGIN,
                 BUTTON_WIDTH, BUTTON_HEIGHT, SWP_NOZORDER);
    SetWindowPos(m_hSettingsButton, nullptr, x + addressBarWidth + BUTTON_WIDTH + BUTTON_MARGIN * 2, BUTTON_MARGIN,
                 BUTTON_WIDTH, BUTTON_HEIGHT, SWP_NOZORDER);
    
    // Resize status bar
    if (m_statusBar) {
        m_statusBar->UpdatePosition();
    }
    
    // Resize WebView
    if (m_webViewController && m_webViewController->IsInitialized()) {
        RECT bounds;
        bounds.left = 0;
        bounds.top = TOOLBAR_HEIGHT;
        bounds.right = rcClient.right;
        bounds.bottom = rcClient.bottom - STATUSBAR_HEIGHT;
        
        m_webViewController->SetBounds(bounds);
    }
}

/**
 * Handle navigation
 */
void MainWindow::OnNavigate()
{
    wchar_t url[2048];
    GetWindowTextW(m_hAddressBar, url, 2048);
    Navigate(url);
}

/**
 * Navigate to URL
 */
void MainWindow::Navigate(const std::wstring& url)
{
    if (m_webViewController && m_webViewController->IsInitialized()) {
        m_webViewController->Navigate(url);
        SetWindowTextW(m_hAddressBar, url.c_str());
    }
}

/**
 * Handle back
 */
void MainWindow::OnBack()
{
    if (m_webViewController && m_webViewController->CanGoBack()) {
        m_webViewController->GoBack();
    }
}

/**
 * Handle forward
 */
void MainWindow::OnForward()
{
    if (m_webViewController && m_webViewController->CanGoForward()) {
        m_webViewController->GoForward();
    }
}

/**
 * Handle refresh
 */
void MainWindow::OnRefresh()
{
    if (m_webViewController) {
        m_webViewController->Refresh();
    }
}

/**
 * Handle home
 */
void MainWindow::OnHome()
{
    Navigate(L"https://www.google.com");
}

/**
 * Set server address
 */
void MainWindow::SetServerAddress(const std::string& serverIp)
{
    m_serverIp = serverIp;
}

/**
 * Start ICMP tunnel
 */
bool MainWindow::StartTunnel()
{
    if (m_tunnelActive) {
        return true;
    }
    
    // Create ICMP session
    m_icmpSession = icmp_session_create(m_serverIp.c_str());
    if (!m_icmpSession) {
        UpdateStatus(L"Failed to create ICMP session");
        return false;
    }
    
    // Start HTTP proxy
    m_httpProxy = http_proxy_create(8888, m_icmpSession);
    if (!m_httpProxy) {
        icmp_session_destroy(m_icmpSession);
        m_icmpSession = nullptr;
        UpdateStatus(L"Failed to create HTTP proxy");
        return false;
    }
    
    if (http_proxy_start(m_httpProxy) != 0) {
        http_proxy_destroy(m_httpProxy);
        icmp_session_destroy(m_icmpSession);
        m_httpProxy = nullptr;
        m_icmpSession = nullptr;
        UpdateStatus(L"Failed to start HTTP proxy");
        return false;
    }
    
    // Configure Windows system proxy to use localhost:8888
    INTERNET_PER_CONN_OPTION_LIST list;
    INTERNET_PER_CONN_OPTION options[2];
    DWORD dwBufferSize = sizeof(list);
    
    // Enable proxy
    options[0].dwOption = INTERNET_PER_CONN_FLAGS;
    options[0].Value.dwValue = PROXY_TYPE_PROXY;
    
    // Set proxy server (use ANSI string for LPSTR)
    options[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    options[1].Value.pszValue = (LPSTR)"http://127.0.0.1:8888";
    
    list.dwSize = sizeof(list);
    list.pszConnection = nullptr; // LAN connection
    list.dwOptionCount = 2;
    list.dwOptionError = 0;
    list.pOptions = options;
    
    InternetSetOption(nullptr, INTERNET_OPTION_PER_CONNECTION_OPTION, &list, dwBufferSize);
    InternetSetOption(nullptr, INTERNET_OPTION_SETTINGS_CHANGED, nullptr, 0);
    InternetSetOption(nullptr, INTERNET_OPTION_REFRESH, nullptr, 0);
    
    m_tunnelActive = true;
    UpdateStatus(L"ICMP tunnel active + System proxy configured");
    
    return true;
}

/**
 * Stop ICMP tunnel
 */
void MainWindow::StopTunnel()
{
    if (!m_tunnelActive) {
        return;
    }
    
    if (m_httpProxy) {
        http_proxy_stop(m_httpProxy);
        http_proxy_destroy(m_httpProxy);
        m_httpProxy = nullptr;
    }
    
    if (m_icmpSession) {
        icmp_session_destroy(m_icmpSession);
        m_icmpSession = nullptr;
    }
    
    m_tunnelActive = false;
    UpdateStatus(L"ICMP tunnel stopped");
}

/**
 * Update status bar
 */
void MainWindow::UpdateStatus(const std::wstring& status)
{
    if (m_statusBar) {
        m_statusBar->SetStatus(status);
    }
}

/**
 * Update connection status
 */
void MainWindow::UpdateConnectionStatus()
{
    if (!m_statusBar) {
        return;
    }
    
    if (m_tunnelActive && m_httpProxy) {
        // Get statistics from HTTP proxy
        uint64_t bytesUp = 0;
        uint64_t bytesDown = 0;
        int latency = 0;
        
        // TODO: Get actual statistics from proxy
        // For now, use placeholder values
        
        m_statusBar->SetConnectionInfo(true, m_serverIp, latency, bytesUp, bytesDown);
    } else {
        m_statusBar->SetConnectionInfo(false, "", 0, 0, 0);
    }
}

/**
 * Handle settings button
 */
void MainWindow::OnSettings()
{
    SettingsDialog dialog(m_hWnd);
    SettingsDialog::Settings newSettings;
    
    if (dialog.Show(m_settings, newSettings)) {
        // Settings were changed
        m_settings = newSettings;
        m_serverIp = m_settings.serverIp;
        
        // Restart tunnel if active
        if (m_tunnelActive) {
            StopTunnel();
            if (StartTunnel()) {
                UpdateStatus(L"Tunnel restarted with new settings");
            }
        } else {
            UpdateStatus(L"Settings saved");
        }
    }
}

/**
 * Handle timer
 */
void MainWindow::OnTimer()
{
    // Update connection status periodically
    UpdateConnectionStatus();
}
