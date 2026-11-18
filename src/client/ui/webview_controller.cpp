/**
 * webview_controller.cpp
 * 
 * WebView2 Controller Implementation
 */

#include "webview_controller.h"
#include <shlwapi.h>
#include <string>

#pragma comment(lib, "shlwapi.lib")

/**
 * Constructor
 */
WebViewController::WebViewController(HWND hwndParent)
    : m_hwndParent(hwndParent)
    , m_initialized(false)
{
}

/**
 * Destructor
 */
WebViewController::~WebViewController()
{
    if (m_controller) {
        m_controller->Close();
    }
}

/**
 * Initialize WebView2
 */
HRESULT WebViewController::Initialize(std::function<void()> onInitialized)
{
    m_onInitialized = onInitialized;
    
    // Create WebView2 environment with options
    wil::com_ptr<ICoreWebView2EnvironmentOptions> options;
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result)) {
                    return result;
                }
                
                // Create WebView2 controller
                env->CreateCoreWebView2Controller(m_hwndParent,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(result)) {
                                return result;
                            }
                            
                            m_controller = controller;
                            m_controller->get_CoreWebView2(&m_webView);
                            
                            // Configure proxy settings
                            wil::com_ptr<ICoreWebView2_2> webView2;
                            m_webView->QueryInterface(IID_PPV_ARGS(&webView2));
                            if (webView2) {
                                wil::com_ptr<ICoreWebView2Settings> settings;
                                m_webView->get_Settings(&settings);
                                
                                wil::com_ptr<ICoreWebView2Settings2> settings2;
                                settings->QueryInterface(IID_PPV_ARGS(&settings2));
                                if (settings2) {
                                    // Set user agent with proxy info
                                    settings2->put_UserAgent(L"ICMPBrowser/1.0");
                                }
                            }
                            
                            // Set bounds
                            RECT bounds;
                            GetClientRect(m_hwndParent, &bounds);
                            m_controller->put_Bounds(bounds);
                            
                            m_initialized = true;
                            
                            // Call initialization callback
                            if (m_onInitialized) {
                                m_onInitialized();
                            }
                            
                            return S_OK;
                        }).Get());
                
                return S_OK;
            }).Get());
    
    return hr;
}

/**
 * Navigate to URL
 */
void WebViewController::Navigate(const std::wstring& url)
{
    if (!m_initialized || !m_webView) {
        return;
    }
    
    m_webView->Navigate(url.c_str());
}

/**
 * Go back
 */
void WebViewController::GoBack()
{
    if (!m_initialized || !m_webView) {
        return;
    }
    
    m_webView->GoBack();
}

/**
 * Go forward
 */
void WebViewController::GoForward()
{
    if (!m_initialized || !m_webView) {
        return;
    }
    
    m_webView->GoForward();
}

/**
 * Refresh
 */
void WebViewController::Refresh()
{
    if (!m_initialized || !m_webView) {
        return;
    }
    
    m_webView->Reload();
}

/**
 * Get current URL
 */
std::wstring WebViewController::GetCurrentUrl()
{
    if (!m_initialized || !m_webView) {
        return L"";
    }
    
    wil::unique_cotaskmem_string uri;
    m_webView->get_Source(&uri);
    
    return std::wstring(uri.get());
}

/**
 * Can go back
 */
bool WebViewController::CanGoBack()
{
    if (!m_initialized || !m_webView) {
        return false;
    }
    
    BOOL canGoBack = FALSE;
    m_webView->get_CanGoBack(&canGoBack);
    
    return canGoBack == TRUE;
}

/**
 * Can go forward
 */
bool WebViewController::CanGoForward()
{
    if (!m_initialized || !m_webView) {
        return false;
    }
    
    BOOL canGoForward = FALSE;
    m_webView->get_CanGoForward(&canGoForward);
    
    return canGoForward == TRUE;
}

/**
 * Set bounds
 */
void WebViewController::SetBounds(RECT bounds)
{
    if (!m_initialized || !m_controller) {
        return;
    }
    
    m_controller->put_Bounds(bounds);
}
