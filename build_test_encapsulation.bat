@echo off
REM Script de compilation pour test_encapsulation_real.exe
REM Nécessite Visual Studio et vcpkg installés

echo ========================================
echo Compilation de test_encapsulation_real
echo ========================================

REM Créer le répertoire build s'il n'existe pas
if not exist "build" mkdir build
cd build

REM Configuration CMake avec vcpkg
echo.
echo [1/3] Configuration CMake...
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" ^
    -DBUILD_TESTS=ON ^
    -DBUILD_CLIENT=ON ^
    -DBUILD_SERVER=OFF

if %ERRORLEVEL% NEQ 0 (
    echo [ERREUR] Configuration CMake echouee
    pause
    exit /b 1
)

REM Compilation
echo.
echo [2/3] Compilation...
cmake --build . --config Release --target test_encapsulation_real

if %ERRORLEVEL% NEQ 0 (
    echo [ERREUR] Compilation echouee
    pause
    exit /b 1
)

REM Copier l'exécutable
echo.
echo [3/3] Copie de l'executable...
if exist "Release\test_encapsulation_real.exe" (
    copy /Y "Release\test_encapsulation_real.exe" "..\bin\test_encapsulation_real.exe"
    echo.
    echo ========================================
    echo [SUCCES] Compilation terminee !
    echo ========================================
    echo.
    echo Executable: bin\test_encapsulation_real.exe
    echo.
    echo Usage: test_encapsulation_real.exe ^<server_ip^> [url]
    echo Example: test_encapsulation_real.exe 192.168.1.100 http://example.com
    echo.
) else (
    echo [ERREUR] Executable non trouve
    pause
    exit /b 1
)

cd ..
pause
