/**
 * webview_controller.h
 * 
 * WebView2 Controller for ICMP Browser
 * Manages Microsoft Edge WebView2 control
 */

#ifndef WEBVIEW_CONTROLLER_H
#define WEBVIEW_CONTROLLER_H

#include <windows.h>
#include <wrl.h>
#include <wil/com.h>
#include <string>
#include "WebView2.h"

using namespace Microsoft::WRL;

class WebViewController {
public:
    /**
     * Constructor
     * 
     * @param hwndParent Parent window handle
     */
    WebViewController(HWND hwndParent);
    
    /**
     * Destructor
     */
    ~WebViewController();
    
    /**
     * Initialize WebView2
     * 
     * @param onInitialized Callback when initialization is complete
     * @return HRESULT status
     */
    HRESULT Initialize(std::function<void()> onInitialized);
    
    /**
     * Navigate to URL
     * 
     * @param url URL to navigate to
     */
    void Navigate(const std::wstring& url);
    
    /**
     * Go back
     */
    void GoBack();
    
    /**
     * Go forward
     */
    void GoForward();
    
    /**
     * Refresh current page
     */
    void Refresh();
    
    /**
     * Get current URL
     * 
     * @return Current URL
     */
    std::wstring GetCurrentUrl();
    
    /**
     * Check if can go back
     * 
     * @return true if can go back
     */
    bool CanGoBack();
    
    /**
     * Check if can go forward
     * 
     * @return true if can go forward
     */
    bool CanGoForward();
    
    /**
     * Set bounds for WebView
     * 
     * @param bounds RECT with new bounds
     */
    void SetBounds(RECT bounds);
    
    /**
     * Check if initialized
     * 
     * @return true if initialized
     */
    bool IsInitialized() const { return m_initialized; }

private:
    HWND m_hwndParent;                          // Parent window
    wil::com_ptr<ICoreWebView2> m_webView;      // WebView2 interface
    wil::com_ptr<ICoreWebView2Controller> m_controller; // WebView2 controller
    bool m_initialized;                          // Initialization flag
    std::function<void()> m_onInitialized;      // Initialization callback
};

#endif /* WEBVIEW_CONTROLLER_H */
