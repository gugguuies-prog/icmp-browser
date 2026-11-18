@echo off
REM Script de compilation pour test_icmp_dll.c avec MSVC ou MinGW

echo === Compilation du test ICMP.DLL ===
echo.

REM Tester si cl.exe (MSVC) est disponible
where cl.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [*] Utilisation de MSVC (cl.exe)
    cl test_icmp_dll.c /Fe:test_icmp_dll.exe /link iphlpapi.lib ws2_32.lib
    if %ERRORLEVEL% EQU 0 (
        echo.
        echo [OK] Compilation reussie avec MSVC!
        goto test
    )
)

REM Tester si gcc (MinGW) est disponible
where gcc.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [*] Utilisation de MinGW (gcc.exe)
    gcc -Wall -O2 test_icmp_dll.c -o test_icmp_dll.exe -liphlpapi -lws2_32
    if %ERRORLEVEL% EQU 0 (
        echo.
        echo [OK] Compilation reussie avec MinGW!
        goto test
    )
)

echo [ERREUR] Aucun compilateur C trouve (MSVC ou MinGW)
echo.
echo Pour installer un compilateur:
echo   - MinGW: https://www.mingw-w64.org/
echo   - MSYS2: https://www.msys2.org/
echo   - Visual Studio Build Tools: https://visualstudio.microsoft.com/downloads/
echo.
pause
exit /b 1

:test
echo.
echo === Lancement du test ===
echo.
test_icmp_dll.exe 8.8.8.8
echo.
pause
