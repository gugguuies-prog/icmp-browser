/**
 * settings_dialog.cpp
 * 
 * Settings Dialog Implementation
 */

#include "settings_dialog.h"
#include <sstream>

// Control IDs
#define IDC_EDIT_SERVER_IP 2001
#define IDC_EDIT_PROXY_PORT 2002
#define IDC_EDIT_TIMEOUT 2003
#define IDC_CHECK_AUTO_CONNECT 2004
#define IDC_CHECK_COMPRESSION 2005
#define IDC_CHECK_STATISTICS 2006
#define IDC_BTN_OK 2007
#define IDC_BTN_CANCEL 2008

/**
 * Constructor
 */
SettingsDialog::SettingsDialog(HWND hwndParent)
    : m_hwndParent(hwndParent)
{
}

/**
 * Show dialog
 */
bool SettingsDialog::Show(const Settings& currentSettings, Settings& newSettings)
{
    m_settings = currentSettings;
    
    // Create dialog manually (no resource file)
    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"#32770",  // Dialog class
        L"Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        m_hwndParent,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );
    
    if (!hDlg) {
        return false;
    }
    
    // Create controls
    int y = 20;
    
    // Server IP
    CreateWindowW(L"STATIC", L"Server IP Address:",
        WS_CHILD | WS_VISIBLE,
        20, y, 150, 20, hDlg, nullptr, nullptr, nullptr);
    CreateWindowW(L"EDIT", std::wstring(currentSettings.serverIp.begin(), 
                                        currentSettings.serverIp.end()).c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        180, y, 180, 20, hDlg, (HMENU)IDC_EDIT_SERVER_IP, nullptr, nullptr);
    y += 30;
    
    // Proxy Port
    CreateWindowW(L"STATIC", L"Proxy Port:",
        WS_CHILD | WS_VISIBLE,
        20, y, 150, 20, hDlg, nullptr, nullptr, nullptr);
    
    wchar_t portStr[16];
    swprintf(portStr, 16, L"%d", currentSettings.proxyPort);
    CreateWindowW(L"EDIT", portStr,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
        180, y, 180, 20, hDlg, (HMENU)IDC_EDIT_PROXY_PORT, nullptr, nullptr);
    y += 30;
    
    // Timeout
    CreateWindowW(L"STATIC", L"Timeout (ms):",
        WS_CHILD | WS_VISIBLE,
        20, y, 150, 20, hDlg, nullptr, nullptr, nullptr);
    
    wchar_t timeoutStr[16];
    swprintf(timeoutStr, 16, L"%d", currentSettings.timeout);
    CreateWindowW(L"EDIT", timeoutStr,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER,
        180, y, 180, 20, hDlg, (HMENU)IDC_EDIT_TIMEOUT, nullptr, nullptr);
    y += 40;
    
    // Checkboxes
    CreateWindowW(L"BUTTON", L"Auto-connect on startup",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        20, y, 340, 20, hDlg, (HMENU)IDC_CHECK_AUTO_CONNECT, nullptr, nullptr);
    if (currentSettings.autoConnect) {
        SendDlgItemMessage(hDlg, IDC_CHECK_AUTO_CONNECT, BM_SETCHECK, BST_CHECKED, 0);
    }
    y += 30;
    
    CreateWindowW(L"BUTTON", L"Enable compression",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        20, y, 340, 20, hDlg, (HMENU)IDC_CHECK_COMPRESSION, nullptr, nullptr);
    if (currentSettings.compression) {
        SendDlgItemMessage(hDlg, IDC_CHECK_COMPRESSION, BM_SETCHECK, BST_CHECKED, 0);
    }
    y += 30;
    
    CreateWindowW(L"BUTTON", L"Show statistics",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        20, y, 340, 20, hDlg, (HMENU)IDC_CHECK_STATISTICS, nullptr, nullptr);
    if (currentSettings.statistics) {
        SendDlgItemMessage(hDlg, IDC_CHECK_STATISTICS, BM_SETCHECK, BST_CHECKED, 0);
    }
    y += 40;
    
    // OK/Cancel buttons
    CreateWindowW(L"BUTTON", L"OK",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        180, y, 80, 25, hDlg, (HMENU)IDC_BTN_OK, nullptr, nullptr);
    
    CreateWindowW(L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        280, y, 80, 25, hDlg, (HMENU)IDC_BTN_CANCEL, nullptr, nullptr);
    
    // Set window procedure
    SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)this);
    SetWindowLongPtr(hDlg, DWLP_DLGPROC, (LONG_PTR)DialogProc);
    
    // Show dialog
    ShowWindow(hDlg, SW_SHOW);
    
    // Message loop
    MSG msg;
    bool result = false;
    
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_COMMAND) {
            if (LOWORD(msg.wParam) == IDC_BTN_OK) {
                GetSettings(hDlg, newSettings);
                result = true;
                DestroyWindow(hDlg);
                break;
            } else if (LOWORD(msg.wParam) == IDC_BTN_CANCEL) {
                result = false;
                DestroyWindow(hDlg);
                break;
            }
        }
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return result;
}

/**
 * Dialog procedure
 */
INT_PTR CALLBACK SettingsDialog::DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    SettingsDialog* dialog = reinterpret_cast<SettingsDialog*>(GetWindowLongPtr(hDlg, GWLP_USERDATA));
    
    switch (message) {
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_BTN_OK) {
                if (dialog) {
                    dialog->GetSettings(hDlg, dialog->m_settings);
                }
                EndDialog(hDlg, IDOK);
                return TRUE;
            } else if (LOWORD(wParam) == IDC_BTN_CANCEL) {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
            break;
            
        case WM_CLOSE:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
    }
    
    return FALSE;
}

/**
 * Get settings from dialog
 */
void SettingsDialog::GetSettings(HWND hDlg, Settings& settings)
{
    // Server IP
    wchar_t buffer[256];
    GetDlgItemTextW(hDlg, IDC_EDIT_SERVER_IP, buffer, 256);
    std::wstring wideIp(buffer);
    settings.serverIp = std::string(wideIp.begin(), wideIp.end());
    
    // Proxy port
    GetDlgItemTextW(hDlg, IDC_EDIT_PROXY_PORT, buffer, 256);
    settings.proxyPort = _wtoi(buffer);
    
    // Timeout
    GetDlgItemTextW(hDlg, IDC_EDIT_TIMEOUT, buffer, 256);
    settings.timeout = _wtoi(buffer);
    
    // Checkboxes
    settings.autoConnect = (SendDlgItemMessage(hDlg, IDC_CHECK_AUTO_CONNECT, BM_GETCHECK, 0, 0) == BST_CHECKED);
    settings.compression = (SendDlgItemMessage(hDlg, IDC_CHECK_COMPRESSION, BM_GETCHECK, 0, 0) == BST_CHECKED);
    settings.statistics = (SendDlgItemMessage(hDlg, IDC_CHECK_STATISTICS, BM_GETCHECK, 0, 0) == BST_CHECKED);
}
