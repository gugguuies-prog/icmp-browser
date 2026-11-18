/**
 * main_window.h
 * 
 * Main Window for ICMP Browser
 * Win32 window with WebView2 integration
 */

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

// Include winsock2 BEFORE windows.h to avoid conflicts
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <windows.h>
#include <string>
#include "status_bar.h"
#include "settings_dialog.h"

// Forward declarations
class WebViewController;
struct http_proxy_t;
struct icmp_session_t;

class MainWindow {
public:
    /**
     * Constructor
     */
    MainWindow();
    
    /**
     * Destructor
     */
    ~MainWindow();
    
    /**
     * Create and show the main window
     * 
     * @param hInstance Application instance handle
     * @param nCmdShow Show window parameter
     * @return true on success, false on error
     */
    bool Create(HINSTANCE hInstance, int nCmdShow);
    
    /**
     * Run the message loop
     * 
     * @return Exit code
     */
    int Run();
    
    /**
     * Get window handle
     * 
     * @return HWND handle
     */
    HWND GetHandle() const { return m_hWnd; }
    
    /**
     * Get WebView controller
     * 
     * @return WebViewController pointer
     */
    WebViewController* GetWebViewController() { return m_webViewController; }
    
    /**
     * Navigate to URL
     * 
     * @param url URL to navigate to
     */
    void Navigate(const std::wstring& url);
    
    /**
     * Set server address for ICMP tunnel
     * 
     * @param serverIp Server IP address
     */
    void SetServerAddress(const std::string& serverIp);
    
    /**
     * Start ICMP tunnel
     * 
     * @return true on success, false on error
     */
    bool StartTunnel();
    
    /**
     * Stop ICMP tunnel
     */
    void StopTunnel();
    
    /**
     * Update status bar
     * 
     * @param status Status message
     */
    void UpdateStatus(const std::wstring& status);
    
    /**
     * Update connection status
     */
    void UpdateConnectionStatus();

private:
    /**
     * Window procedure
     */
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    
    /**
     * Initialize WebView2
     */
    void InitializeWebView();
    
    /**
     * Create UI controls
     */
    void CreateControls();
    
    /**
     * Handle resize
     */
    void OnResize();
    
    /**
     * Handle navigation
     */
    void OnNavigate();
    
    /**
     * Handle back button
     */
    void OnBack();
    
    /**
     * Handle forward button
     */
    void OnForward();
    
    /**
     * Handle refresh button
     */
    void OnRefresh();
    
    /**
     * Handle home button
     */
    void OnHome();
    
    /**
     * Handle settings button
     */
    void OnSettings();
    
    /**
     * Handle timer
     */
    void OnTimer();

private:
    HWND m_hWnd;                        // Main window handle
    HWND m_hAddressBar;                 // Address bar edit control
    HWND m_hGoButton;                   // Go button
    HWND m_hBackButton;                 // Back button
    HWND m_hForwardButton;              // Forward button
    HWND m_hRefreshButton;              // Refresh button
    HWND m_hHomeButton;                 // Home button
    HWND m_hSettingsButton;             // Settings button
    
    StatusBar* m_statusBar;             // Enhanced status bar
    WebViewController* m_webViewController; // WebView2 controller
    http_proxy_t* m_httpProxy;          // HTTP proxy instance
    icmp_session_t* m_icmpSession;      // ICMP tunnel session
    
    SettingsDialog::Settings m_settings; // Application settings
    std::string m_serverIp;             // Server IP address
    bool m_tunnelActive;                // Tunnel active flag
    UINT_PTR m_timerID;                 // Timer for status updates
    
    static const int TOOLBAR_HEIGHT = 60;
    static const int STATUSBAR_HEIGHT = 30;
    static const int BUTTON_WIDTH = 60;
    static const int BUTTON_HEIGHT = 30;
    static const int BUTTON_MARGIN = 5;
};

#endif /* MAIN_WINDOW_H */
