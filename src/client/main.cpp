/**
 * main.cpp
 * 
 * ICMP Browser - Main Entry Point
 * Windows application using WebView2 for browsing via ICMP tunnel
 */

// IMPORTANT: winsock2.h MUST be included BEFORE windows.h
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include "ui/main_window.h"

/**
 * Parse command line arguments
 */
bool ParseCommandLine(int argc, wchar_t* argv[], std::string& serverIp)
{
    for (int i = 1; i < argc; i++) {
        std::wstring arg = argv[i];
        
        if (arg == L"--server" || arg == L"-s") {
            if (i + 1 < argc) {
                // Convert wstring to string
                std::wstring wideServer = argv[i + 1];
                serverIp = std::string(wideServer.begin(), wideServer.end());
                return true;
            }
        }
        else if (arg == L"--help" || arg == L"-h") {
            MessageBoxW(nullptr, 
                       L"ICMP Browser - Portable Web Browser using ICMP Tunnel\n\n"
                       L"Usage: icmpbrowser.exe [OPTIONS]\n\n"
                       L"Options:\n"
                       L"  --server, -s <IP>    Server IP address\n"
                       L"  --help, -h           Show this help message\n\n"
                       L"Example:\n"
                       L"  icmpbrowser.exe --server 192.168.1.100",
                       L"ICMP Browser Help",
                       MB_OK | MB_ICONINFORMATION);
            return false;
        }
    }
    
    return false;
}

/**
 * WinMain - Windows application entry point
 */
int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    
    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        MessageBoxW(nullptr, L"Failed to initialize COM", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Parse command line
    int argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    std::string serverIp;
    bool hasServer = ParseCommandLine(argc, argv, serverIp);
    
    LocalFree(argv);
    
    // Create main window
    MainWindow mainWindow;
    
    if (!mainWindow.Create(hInstance, nCmdShow)) {
        MessageBoxW(nullptr, L"Failed to create main window", L"Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }
    
    // If server was provided, start tunnel
    if (hasServer) {
        mainWindow.SetServerAddress(serverIp);
        
        if (!mainWindow.StartTunnel()) {
            MessageBoxW(nullptr, 
                       L"Failed to start ICMP tunnel.\n"
                       L"Make sure the server is reachable and you have network access.",
                       L"Error", 
                       MB_OK | MB_ICONERROR);
        }
    } else {
        // Show info about configuring server
        MessageBoxW(nullptr,
                   L"Welcome to ICMP Browser!\n\n"
                   L"To use the ICMP tunnel, restart with:\n"
                   L"icmpbrowser.exe --server <IP>\n\n"
                   L"For now, you can browse normally (no tunnel).",
                   L"ICMP Browser",
                   MB_OK | MB_ICONINFORMATION);
    }
    
    // Run message loop
    int exitCode = mainWindow.Run();
    
    // Cleanup
    CoUninitialize();
    
    return exitCode;
}
