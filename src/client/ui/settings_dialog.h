/**
 * settings_dialog.h
 * 
 * Settings Dialog for ICMP Browser
 * Configure server, proxy, and connection settings
 */

#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <windows.h>
#include <string>

class SettingsDialog {
public:
    /**
     * Settings structure
     */
    struct Settings {
        std::string serverIp;           // Server IP address
        int proxyPort;                  // Proxy port (default 8888)
        int timeout;                    // Connection timeout (ms)
        bool autoConnect;               // Auto-connect on startup
        bool compression;               // Enable compression
        bool statistics;                // Show statistics
    };
    
    /**
     * Constructor
     */
    SettingsDialog(HWND hwndParent);
    
    /**
     * Show dialog
     * 
     * @param currentSettings Current settings
     * @param newSettings Output: new settings if OK clicked
     * @return true if OK clicked, false if cancelled
     */
    bool Show(const Settings& currentSettings, Settings& newSettings);

private:
    /**
     * Dialog procedure
     */
    static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    
    /**
     * Initialize dialog controls
     */
    void InitializeControls(HWND hDlg, const Settings& settings);
    
    /**
     * Get settings from dialog controls
     */
    void GetSettings(HWND hDlg, Settings& settings);

private:
    HWND m_hwndParent;
    Settings m_settings;
};

#endif /* SETTINGS_DIALOG_H */
