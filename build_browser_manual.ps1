# Script de compilation manuelle du navigateur ICMP Browser
# Évite les conflits d'en-têtes Windows

Write-Host "=== Compilation du navigateur ICMP Browser ===" -ForegroundColor Cyan

# Trouver le compilateur MSVC
$vsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community"
if (!(Test-Path $vsPath)) {
    $vsPath = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
}

if (!(Test-Path $vsPath)) {
    Write-Host "Visual Studio 2022 introuvable!" -ForegroundColor Red
    exit 1
}

# Charger l'environnement VS
$vcvars = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vcvars`" && set" | ForEach-Object {
    if ($_ -match "^(.*?)=(.*)$") {
        Set-Item -Path "Env:$($matches[1])" -Value $matches[2]
    }
}

# Créer le dossier de sortie
$outDir = "bin"
if (!(Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir | Out-Null
}

Write-Host "Compilation en cours..." -ForegroundColor Yellow

# Compiler avec cl.exe
$sources = @(
    "src\browser\icmp_browser_simple.cpp",
    "src\client\icmp_engine.c",
    "src\common\protocol.c",
    "src\common\handshake.c",
    "src\common\compression.c",
    "src\common\statistics.c",
    "src\common\priority_queue.c",
    "src\crypto\aes_gcm.c",
    "src\crypto\diffie_hellman.c"
)

$includes = @(
    "/Isrc",
    "/IC:\vcpkg\installed\x64-windows\include"
)

$defines = @(
    "/DWIN32",
    "/D_WINDOWS",
    "/DUNICODE",
    "/D_UNICODE",
    "/DWIN32_LEAN_AND_MEAN"
)

$libs = @(
    "C:\vcpkg\installed\x64-windows\lib\libcrypto.lib",
    "C:\vcpkg\installed\x64-windows\lib\libssl.lib",
    "C:\vcpkg\installed\x64-windows\lib\zlib.lib",
    "ws2_32.lib",
    "iphlpapi.lib",
    "comctl32.lib",
    "user32.lib",
    "gdi32.lib"
)

$clArgs = @(
    "/nologo",
    "/W3",
    "/O2",
    "/MD",
    "/EHsc",
    "/std:c++17"
) + $includes + $defines + $sources + @(
    "/link",
    "/OUT:bin\ICMPBrowser.exe",
    "/SUBSYSTEM:WINDOWS"
) + $libs

& cl.exe $clArgs 2>&1 | Write-Host

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "  Compilation réussie!" -ForegroundColor Green
    Write-Host "  Exécutable: bin\ICMPBrowser.exe" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "Erreur de compilation!" -ForegroundColor Red
    exit 1
}
