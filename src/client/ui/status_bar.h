/**
 * status_bar.h
 * 
 * Enhanced Status Bar with Connection Info
 */

#ifndef STATUS_BAR_H
#define STATUS_BAR_H

#include <windows.h>
#include <string>

class StatusBar {
public:
    /**
     * Constructor
     */
    StatusBar(HWND hwndParent);
    
    /**
     * Destructor
     */
    ~StatusBar();
    
    /**
     * Create status bar
     */
    bool Create();
    
    /**
     * Set status text
     */
    void SetStatus(const std::wstring& status);
    
    /**
     * Set connection info
     */
    void SetConnectionInfo(bool connected, const std::string& serverIp, 
                          int latency, uint64_t bytesUp, uint64_t bytesDown);
    
    /**
     * Update position/size
     */
    void UpdatePosition();
    
    /**
     * Get window handle
     */
    HWND GetHandle() const { return m_hWnd; }

private:
    HWND m_hwndParent;
    HWND m_hWnd;
    std::wstring m_statusText;
    bool m_connected;
    std::string m_serverIp;
    int m_latency;
    uint64_t m_bytesUp;
    uint64_t m_bytesDown;
};

#endif /* STATUS_BAR_H */
