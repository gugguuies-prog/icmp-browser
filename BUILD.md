# 🔨 ICMP Browser - Build Instructions

## 📋 Prérequis

### Windows (Client)

**Obligatoire:**
- Visual Studio 2019 ou plus récent (Community Edition suffit)
- CMake 3.15+
- OpenSSL for Windows
- WebView2 SDK

**Installation WebView2 SDK:**

```powershell
# Option 1: Via NuGet Package Manager
Install-Package Microsoft.Web.WebView2 -Version 1.0.2210.55

# Option 2: Téléchargement manuel
# https://www.nuget.org/packages/Microsoft.Web.WebView2
```

**Installation OpenSSL:**

```powershell
# Via vcpkg (recommandé)
vcpkg install openssl:x64-windows
vcpkg install zlib:x64-windows

# Ou télécharger depuis: https://slproweb.com/products/Win32OpenSSL.html
```

### Linux (Server)

**Obligatoire:**
- GCC 7+ ou Clang 6+
- CMake 3.15+
- OpenSSL development libraries
- zlib development libraries
- pthread

**Installation Ubuntu/Debian:**

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libssl-dev \
    zlib1g-dev \
    libpthread-stubs0-dev
```

**Installation Armbian:**

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libssl-dev \
    zlib1g-dev
```

---

## 🔨 Compilation

### Windows Client

#### Option 1: Visual Studio (Recommandé)

1. **Ouvrir Visual Studio**
2. **File → Open → CMake** → Sélectionner `CMakeLists.txt`
3. **Build → Build All**

L'exécutable sera dans : `out\build\x64-Release\icmpbrowser.exe`

#### Option 2: Command Line

```powershell
# Créer dossier de build
mkdir build
cd build

# Configurer CMake
cmake .. -G "Visual Studio 16 2019" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"

# Compiler
cmake --build . --config Release

# Résultat
.\Release\icmpbrowser.exe
```

#### Option 3: MinGW (Cross-compile depuis WSL)

```bash
# Dans WSL
cd "/mnt/c/Users/YOUR_USERNAME/Documents/code/ICMP Browser/icmptunnel"

# Utiliser le Makefile existant
make test-http-proxy-win  # Tests
# TODO: Add client target to Makefile
```

---

### Linux Server

```bash
# Créer dossier de build
mkdir build
cd build

# Configurer CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Compiler
make -j$(nproc)

# Tests
ctest --output-on-failure
```

---

### Compilation des Tests Seulement

#### Windows Tests

```bash
# Via WSL + MinGW
wsl make test-http-proxy-linux
wsl make test-compression-linux
wsl make test-statistics-linux
# ... etc
```

#### Linux Tests

```bash
# Build directory
cd build

# Compiler tous les tests
make -j$(nproc)

# Lancer tests individuels
./test_aes_gcm
./test_dh
./test_protocol
./test_handshake
./test_session_manager
./test_tun_device  # Nécessite root
./test_priority_queue
./test_compression
./test_statistics
./test_http_proxy
```

---

## 🚀 Lancer l'Application

### Windows Client

**Avec tunnel ICMP:**

```powershell
.\icmpbrowser.exe --server 192.168.1.100
```

**Sans tunnel (navigation normale):**

```powershell
.\icmpbrowser.exe
```

**Aide:**

```powershell
.\icmpbrowser.exe --help
```

---

### Linux Server

**Préparer le serveur:**

```bash
# Setup réseau (nécessite root)
sudo ./scripts/server_setup.sh

# Si Armbian
sudo ./scripts/armbian_specific.sh
```

**Lancer le serveur:**

```bash
# TODO: Server program not yet implemented
# Pour l'instant, seule la bibliothèque serveur est compilée
```

---

## 🧪 Vérifier l'Installation

### Tests Rapides

**Windows:**

```powershell
# Test proxy HTTP
.\build\Release\test_http_proxy.exe

# Test ICMP engine
.\build\Release\test_icmp_engine.exe
```

**Linux:**

```bash
# Tous les tests
cd build
ctest

# Tests individuels
./test_http_proxy     # Proxy HTTP
./test_compression    # Compression
./test_statistics     # Statistiques
```

---

## 🐛 Problèmes Courants

### Windows

**"WebView2 SDK not found"**
```powershell
# Installer via NuGet ou télécharger manuellement
Install-Package Microsoft.Web.WebView2
```

**"OpenSSL not found"**
```powershell
# Via vcpkg
vcpkg install openssl:x64-windows
vcpkg integrate install
```

**"Cannot find iphlpapi.lib"**
```
Solution: S'assurer d'avoir Windows SDK installé avec Visual Studio
```

### Linux

**"libssl.so not found"**
```bash
sudo apt-get install libssl-dev
```

**"test_tun_device fails"**
```bash
# Nécessite root pour créer TUN device
sudo ./test_tun_device
```

---

## 📦 Structure du Projet

```
icmptunnel/
├── CMakeLists.txt          # Configuration CMake
├── Makefile                # Makefile pour tests (MinGW)
├── BUILD.md                # Ce fichier
├── src/
│   ├── client/             # Code client Windows
│   │   ├── icmp_engine.c   # ICMP Windows (Icmp.dll)
│   │   ├── http_proxy.c    # Proxy HTTP
│   │   ├── main.cpp        # Entry point
│   │   └── ui/             # Interface WebView2
│   ├── server/             # Code serveur Linux
│   │   ├── icmp_handler.c  # ICMP raw sockets
│   │   ├── session_manager.c
│   │   └── tun_device.c    # Interface TUN
│   ├── common/             # Code partagé
│   │   ├── protocol.c      # Protocole ICMP tunnel
│   │   ├── handshake.c     # DH handshake
│   │   ├── priority_queue.c
│   │   ├── compression.c
│   │   └── statistics.c
│   └── crypto/             # Cryptographie
│       ├── aes_gcm.c       # AES-256-GCM
│       └── diffie_hellman.c # DH-2048
├── tests/                  # Tests unitaires
└── scripts/                # Scripts setup serveur
```

---

## 📈 Statut de Compilation

✅ **Compilable:**
- Tous les modules C (crypto, protocol, common)
- Tests unitaires (94 tests)
- Proxy HTTP (Windows + Linux)

🔄 **En cours:**
- Application Windows complète (WebView2)
- Intégration CMake

⏸️ **À venir:**
- Programme serveur complet
- Packaging (installer Windows)

---

## 💡 Pour les Développeurs

### Ajouter un Nouveau Module

1. Créer `src/module/mon_module.c` et `.h`
2. Ajouter au `CMakeLists.txt` dans la section appropriée
3. Créer `tests/test_mon_module.c`
4. Ajouter le test avec `add_icmp_test()`

### Compiler en Debug

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
gdb ./test_mon_module
```

### Profiling

```bash
# Linux
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
make
valgrind --leak-check=full ./icmpbrowser
```

---

## 📞 Support

**Documentation:**
- `PROGRESS.md` - Statut détaillé du projet
- `PROJECT_STATUS.md` - Vue d'ensemble
- `README.md` - Guide utilisateur

**Tests:**
- 94 tests unitaires disponibles
- Exécuter avec `make test` ou `ctest`

---

**Dernière mise à jour:** 2024-11-17
