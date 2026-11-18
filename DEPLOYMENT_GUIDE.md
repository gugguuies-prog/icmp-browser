# 🚀 Guide de Déploiement - ICMP Browser

**Date:** 2024-11-17  
**Version:** 1.0.0-RC

---

## 📋 Vue d'Ensemble

Ce guide te permet de déployer le serveur ICMP sur ton Armbian et de compiler le client Windows.

---

## 🔧 Partie 1: Serveur Armbian (MAINTENANT)

### Étape 1: Connexion SSH

```bash
ssh mgmine@192.168.1.40
```

### Étape 2: Lancer le script d'installation

```bash
cd ~/icmptunnel-build
./armbian_install.sh
```

**Le script va:**
1. ✅ Mettre à jour apt
2. ✅ Installer: build-essential, gcc, g++, libssl-dev, zlib1g-dev
3. ✅ Compiler le serveur ICMP
4. ✅ Compiler les tests unitaires

**Temps estimé:** 5-10 minutes

### Étape 3: Tester le serveur

```bash
cd ~/icmptunnel-build

# Test 1: ICMP Handler
sudo ./bin/test_icmp_handler

# Test 2: Session Manager
./bin/test_session_manager

# Test 3: TUN Device (nécessite root)
sudo ./bin/test_tun_device
```

### Étape 4: Configuration réseau

```bash
# Setup complet (TUN device, NAT, iptables)
cd ~/icmptunnel-build
sudo ./scripts/server_setup.sh

# Vérifier que tout est configuré
ip addr show tun_icmp0  # Devrait montrer 10.0.1.1/24
cat /proc/sys/net/ipv4/ip_forward  # Devrait afficher 1
```

### Étape 5: Lancer le serveur (TODO)

**⚠️ PROBLÈME:** Nous n'avons pas encore de programme `main()` pour le serveur !

**Solution temporaire:** Les tests peuvent servir de serveur pour validation.

**TODO pour Phase 6:**
- Créer `src/server/main.c` avec boucle principale
- Compiler en tant qu'exécutable serveur standalone

---

## 💻 Partie 2: Client Windows (TOI)

### Option A: Avec Build Tools for Visual Studio (Recommandé)

#### Étape 1: Télécharger Build Tools

1. Aller sur: https://visualstudio.microsoft.com/downloads/
2. Scroll down jusqu'à "Tools for Visual Studio"
3. Télécharger "Build Tools for Visual Studio 2022"
4. Taille: ~1.5 GB

#### Étape 2: Installer Build Tools

1. Lancer `vs_BuildTools.exe`
2. Cocher "Desktop development with C++"
3. Dans l'onglet "Individual components", vérifier:
   - ✅ MSVC v143 - VS 2022 C++ x64/x86 build tools
   - ✅ Windows 11 SDK (ou 10)
   - ✅ CMake tools for Windows
   - ✅ C++ ATL for latest build tools
4. Installer (prend ~30 minutes)

#### Étape 3: Installer vcpkg

```powershell
# Dans PowerShell
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

#### Étape 4: Installer dépendances

```powershell
cd C:\vcpkg
.\vcpkg install openssl:x64-windows
.\vcpkg install zlib:x64-windows  
.\vcpkg install wil:x64-windows
```

#### Étape 5: Installer WebView2 SDK

**Option 1: Via NuGet (dans VS Code)**
1. Installer extension "NuGet Package Manager"
2. Ouvrir Command Palette (Ctrl+Shift+P)
3. "NuGet: Install Package"
4. Chercher "Microsoft.Web.WebView2"
5. Installer

**Option 2: Téléchargement manuel**
1. https://developer.microsoft.com/en-us/microsoft-edge/webview2/
2. "Download SDK" → "Fixed Version"
3. Extraire dans `C:\WebView2SDK`

#### Étape 6: Compiler avec CMake

```powershell
cd "C:\Users\mgmin\Documents\code\ICMP Browser\icmptunnel"
mkdir build-windows
cd build-windows

# Générer projet
cmake .. -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DBUILD_CLIENT=ON -DBUILD_SERVER=OFF

# Compiler
cmake --build . --config Release

# Tester
.\Release\icmpbrowser.exe --help
```

#### Étape 7: Lancer l'application

```powershell
.\Release\icmpbrowser.exe --server 192.168.1.40
```

**Ce qui devrait se passer:**
1. ✅ Fenêtre s'ouvre avec WebView2
2. ✅ Toolbar avec boutons navigation
3. ✅ Status bar affiche "🔴 Disconnected"
4. ✅ HTTP Proxy démarre sur localhost:8888
5. ✅ Tentative de connexion ICMP vers 192.168.1.40
6. ✅ Si serveur actif: Handshake DH, Status bar "🟢 Connected"

---

### Option B: Client CLI Temporaire (Plus Simple)

Si Build Tools est trop compliqué, je peux créer un client CLI dans WSL qui:
- Établit le tunnel ICMP
- Lance un proxy HTTP
- Tu utilises un navigateur normal pointé sur le proxy

**Avantages:**
- ✅ Pas besoin de Visual Studio
- ✅ Compile dans WSL avec gcc
- ✅ Fonctionne immédiatement

**Inconvénients:**
- ❌ Pas d'interface graphique intégrée
- ❌ Nécessite navigateur séparé
- ❌ Pas aussi "portable"

---

## 🧪 Phase 6: Tests End-to-End

### Test 1: Connectivité basique

**Sur Armbian:**
```bash
# Lancer serveur (simulation avec test)
sudo ~/icmptunnel-build/bin/test_icmp_handler &
```

**Sur Windows (client):**
```powershell
# Tester ping ICMP
ping 192.168.1.40

# Lancer client (quand compilé)
.\icmpbrowser.exe --server 192.168.1.40
```

### Test 2: Tunnel ICMP

1. Client établit connexion ICMP
2. Handshake Diffie-Hellman
3. Clé de session AES-256 dérivée
4. Status bar affiche "🟢 Connected"

### Test 3: Navigation web

1. Taper URL dans address bar: `https://www.google.com`
2. Requête → Proxy (localhost:8888) → ICMP tunnel → Serveur → Internet
3. Réponse revient via même chemin
4. Page s'affiche dans WebView2

### Test 4: Performance

- Mesurer latency (RTT)
- Mesurer throughput (download/upload)
- Tester compression
- Tester stabilité longue durée

---

## 📊 État Actuel

### ✅ Fait
- Backend complet (crypto, ICMP, protocole)
- Tests unitaires (101 tests)
- Serveur Linux (code complet)
- Client Windows (code complet)
- CMake build system
- Documentation complète

### 🔄 En Cours
- **Compilation serveur Armbian** ← TOI MAINTENANT
- Compilation client Windows ← TOI APRÈS

### ⏸️ À Faire
- Créer `src/server/main.c` (programme serveur standalone)
- Tests E2E complets
- Performance tuning
- Packaging
- Documentation finale

---

## ❓ Problèmes Possibles

### Serveur

**Problème:** `permission denied` lors des tests
**Solution:** Utiliser `sudo` pour les tests ICMP/TUN

**Problème:** `module tun not found`
**Solution:** `sudo modprobe tun`

**Problème:** `iptables rules fail`
**Solution:** Vérifier que tu as les droits root

### Client

**Problème:** WebView2 ne se charge pas
**Solution:** Installer WebView2 Runtime: https://go.microsoft.com/fwlink/p/?LinkId=2124703

**Problème:** `openssl.dll not found`
**Solution:** Copier DLLs depuis `C:\vcpkg\installed\x64-windows\bin\` vers le dossier exe

**Problème:** `Cannot open include file: 'WebView2.h'`
**Solution:** Réinstaller WebView2 SDK et vérifier CMAKE_PREFIX_PATH

---

## 📞 Support

**Documentation:**
- `BUILD.md` - Instructions compilation détaillées
- `STATUS_NOW.md` - État actuel du projet
- `PROGRESS.md` - Progression complète
- `SESSION_3.md` - Dernière session de travail

**Fichiers clés:**
- Serveur: `~/icmptunnel-build/` sur Armbian
- Client: `C:\Users\mgmin\Documents\code\ICMP Browser\icmptunnel\`
- Tests: `bin/test_*` et `build-linux/test_*`

---

## 🎯 Objectif Final

**Avoir un navigateur Windows portable qui:**
1. ✅ Fonctionne SANS droits administrateur
2. ✅ Établit tunnel ICMP crypté (AES-256-GCM)
3. ✅ Navigue sur Internet via serveur Armbian
4. ✅ Interface moderne avec WebView2 (Chromium)
5. ✅ Status bar avec stats en temps réel
6. ✅ Portable (un seul exe + DLLs)

---

**Dernière mise à jour:** 2024-11-17 20:30  
**Prochaine étape:** Exécuter `./armbian_install.sh` sur ton Armbian ! 🚀
