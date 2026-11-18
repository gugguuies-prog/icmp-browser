# 🌐 ICMP Browser - Portable Windows Browser via ICMP Tunnel

**Version:** 1.0.0-RC  
**Status:** Beta - Ready for Testing  

---

## 📖 Description

Un navigateur Windows **portable** qui utilise un tunnel ICMP pour contourner les restrictions réseau, **SANS nécessiter de droits administrateur**.

### ✨ Caractéristiques

- ✅ **100% Portable** - Aucune installation requise
- ✅ **Sans Droits Admin** - Utilise l'API Windows Icmp.dll
- ✅ **Crypté AES-256-GCM** - Sécurité maximale
- ✅ **Handshake Diffie-Hellman 2048-bit** - Échange de clés sécurisé
- ✅ **Interface Moderne** - WebView2 (Chromium Engine)
- ✅ **Compression zlib** - Optimisation bande passante
- ✅ **Stats Temps Réel** - Latence, débit, connexion

---

## 🏗️ Architecture

```
┌──────────────────────────┐         ICMP Packets         ┌────────────────┐
│   Windows Client         │◄────────(Encrypted)──────────►│ Linux Server   │
│   (No Admin Required)    │                               │ (Armbian/Debian)│
│                          │                               │                │
│  ┌────────────────────┐  │                               │ ┌────────────┐ │
│  │  WebView2 Browser  │  │                               │ │TUN Device  │ │
│  └────────┬───────────┘  │                               │ │10.0.1.1/24 │ │
│           │ HTTP         │                               │ └─────┬──────┘ │
│  ┌────────▼───────────┐  │                               │       │        │
│  │ HTTP Proxy :8888   │  │                               │   NAT/FW       │
│  └────────┬───────────┘  │                               │       │        │
│           │              │                               │       ▼        │
│  ┌────────▼───────────┐  │                               │   Internet     │
│  │ ICMP Engine        │──┼───────────────────────────────┤               │
│  │ (AES-256 + DH)     │  │                               │               │
│  └────────────────────┘  │                               └───────────────┘
└──────────────────────────┘
```

---

## 🚀 Quick Start

### Client Windows

#### Option 1: Binaire Précompilé (À venir)

```powershell
# Télécharger release
# Extraire archive
# Lancer
.\icmpbrowser.exe --server 192.168.1.100
```

#### Option 2: Compiler Soi-Même

**Prérequis:**
- Windows 10/11
- Visual Studio Build Tools 2022
- CMake 3.15+
- vcpkg

**Compilation:**

```powershell
# 1. Cloner repo
git clone https://github.com/gugguuies-prog/icmp-browser.git
cd icmp-browser

# 2. Installer vcpkg
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# 3. Installer dépendances
.\vcpkg install openssl:x64-windows
.\vcpkg install zlib:x64-windows
.\vcpkg install wil:x64-windows

# 4. Installer WebView2 SDK
# Via NuGet: Microsoft.Web.WebView2
# Ou: https://developer.microsoft.com/microsoft-edge/webview2/

# 5. Compiler
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build . --config Release

# 6. Lancer
.\Release\icmpbrowser.exe --server <SERVER_IP>
```

### Serveur Linux (Armbian/Debian/Ubuntu)

```bash
# 1. Cloner repo
git clone https://github.com/gugguuies-prog/icmp-browser.git
cd icmp-browser

# 2. Installer et compiler
chmod +x armbian_install.sh
./armbian_install.sh

# 3. Configurer réseau
sudo ./scripts/server_setup.sh

# 4. Lancer serveur (TODO: main program)
sudo ./bin/test_icmp_handler
```

---

## 📚 Documentation

- **[BUILD.md](BUILD.md)** - Instructions compilation détaillées
- **[DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md)** - Guide déploiement complet
- **[STATUS_NOW.md](STATUS_NOW.md)** - État actuel du projet
- **[PROGRESS.md](PROGRESS.md)** - Progression détaillée

---

## 🧪 Tests

**101 tests unitaires - 100% passants**

```bash
# Dans build/
ctest -C Release

# Ou individuellement:
./test_aes_gcm
./test_http_proxy
./test_icmp_handler  # Nécessite root
```

---

## 📊 Statistiques Projet

**Code:**
- 10,100 lignes totales
- 5,500 lignes C (backend)
- 2,200 lignes C++ (frontend)
- 2,400 lignes de tests

**Modules:**
- Crypto (AES-256-GCM, DH-2048)
- ICMP (Client Windows, Server Linux)
- Protocole (Encapsulation, Handshake)
- HTTP Proxy (Multi-threadé)
- UI (WebView2, StatusBar, Settings)

**Progression: 82.5% (16.5/20 tâches)**

---

## ⚠️ Limitations & TODO

### En Cours
- [ ] Programme serveur standalone (`src/server/main.c`)
- [ ] Tests end-to-end complets
- [ ] Packaging (installeur Windows, .deb Linux)

### Limitations Connues
- Throughput limité par ICMP (~1-5 Mbps)
- Certains ISP peuvent bloquer/limiter ICMP
- Latency plus élevée que connexion directe

---

## 🔒 Sécurité

- **AES-256-GCM** - Chiffrement authentifié
- **DH-2048** - Échange de clés Diffie-Hellman
- **HKDF** - Dérivation de clés sécurisée
- **Session Keys** - Clés uniques par session
- **No Admin** - Pas de privilèges élevés requis

---

## 📝 Licence

MIT License - Voir [LICENSE](LICENSE)

---

## 🤝 Contribution

Contributions bienvenues ! Ouvrez une issue ou PR.

### Structure Projet

```
icmp-browser/
├── src/
│   ├── client/          # Client Windows
│   │   ├── http_proxy.c # Proxy HTTP
│   │   ├── icmp_engine.c# ICMP Engine
│   │   └── ui/          # Interface WebView2
│   ├── server/          # Serveur Linux
│   │   ├── icmp_handler.c
│   │   ├── session_manager.c
│   │   └── tun_device.c
│   ├── common/          # Code partagé
│   │   ├── protocol.c
│   │   ├── handshake.c
│   │   └── compression.c
│   └── crypto/          # Cryptographie
│       ├── aes_gcm.c
│       └── diffie_hellman.c
├── tests/               # Tests unitaires
├── scripts/             # Scripts serveur
├── CMakeLists.txt       # Build system
└── docs/                # Documentation
```

---

## 👤 Auteur

**gugguuies-prog**

---

## 🙏 Remerciements

- Projet inspiré de [icmptunnel](https://github.com/DhavalKapil/icmptunnel)
- OpenSSL pour la crypto
- Microsoft pour WebView2

---

**⭐ Si ce projet vous aide, n'hésitez pas à lui donner une étoile !**
