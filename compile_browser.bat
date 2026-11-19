@echo off
echo Compilation du navigateur ICMP Browser...

REM Trouver cl.exe (compilateur MSVC)
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

REM Dossier de sortie
if not exist "bin" mkdir bin

REM Compiler
echo Compilation en cours...
cl.exe /W3 /O2 /MD /EHsc /std:c++17 ^
    /I"src" ^
    /I"C:\vcpkg\installed\x64-windows\include" ^
    /DWIN32 /D_WINDOWS /DUNICODE /D_UNICODE ^
    src\browser\icmp_browser_simple.cpp ^
    src\client\icmp_engine.c ^
    src\common\protocol.c ^
    src\common\handshake.c ^
    src\common\compression.c ^
    src\common\statistics.c ^
    src\common\priority_queue.c ^
    src\crypto\aes_gcm.c ^
    src\crypto\diffie_hellman.c ^
    /link ^
    /OUT:bin\ICMPBrowser.exe ^
    /SUBSYSTEM:WINDOWS ^
    /LIBPATH:"C:\vcpkg\installed\x64-windows\lib" ^
    libcrypto.lib libssl.lib zlib.lib ^
    ws2_32.lib iphlpapi.lib comctl32.lib user32.lib gdi32.lib ^
    /ENTRY:wWinMainCRTStartup

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo   Compilation reussie!
    echo   Executable: bin\ICMPBrowser.exe
    echo ========================================
) else (
    echo.
    echo Erreur de compilation!
)

pause
