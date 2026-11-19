/**
 * browser_window.h
 * 
 * Modern browser window interface for ICMP Browser
 */

#ifndef BROWSER_WINDOW_H
#define BROWSER_WINDOW_H

#include <windows.h>
#include <string>
#include <functional>

/**
 * Browser window class - Modern Chromium-style interface
 */
class BrowserWindow {
public:
    BrowserWindow();
    ~BrowserWindow();
    
    // Initialize the browser window
    bool Initialize(HINSTANCE hInstance);
    
    // Show the window
    void Show();
    
    // Run the message loop
    int Run();
    
    // Navigate to URL
    void Navigate(const std::wstring& url);
    
    // Set callback for URL requests (intercept HTTP requests to use ICMP tunnel)
    void SetRequestCallback(std::function<std::string(const std::string&)> callback);
    
private:
    HWND m_hwnd;
    HWND m_urlBar;
    HWND m_goButton;
    HWND m_backButton;
    HWND m_forwardButton;
    HWND m_refreshButton;
    HWND m_contentArea;
    
    HINSTANCE m_hInstance;
    std::function<std::string(const std::string&)> m_requestCallback;
    
    // Window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // Handle commands (buttons, etc.)
    void OnCommand(WPARAM wParam);
    
    // Handle resize
    void OnResize();
    
    // Load URL through ICMP tunnel
    void LoadUrl(const std::wstring& url);
};

#endif // BROWSER_WINDOW_H
