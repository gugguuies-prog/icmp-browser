# ICMP HTTP Proxy - Guide de compilation et utilisation

## 🎯 Qu'est-ce que c'est?

Un proxy HTTP standalone qui tunnel TOUT le trafic web à travers ICMP.
- ✅ Marche avec **n'importe quel navigateur** (Chrome, Firefox, Edge)
- ✅ Pas besoin de CEF ou WebView2
- ✅ Streaming automatique par chunks (1200 bytes)
- ✅ Supporte les gros fichiers (jusqu'à 65KB par requête)

## 📦 Compilation

### Sur ta VM Windows:

```powershell
cd c:\icmp-browser
git pull
cd build-cef
cmake --build . --config Release --target icmp_http_proxy
```

L'exécutable sera créé dans: `c:\icmp-browser\build-cef\Release\icmp_http_proxy.exe`

## 🚀 Utilisation

### 1. Lance le serveur sur Orange Pi

```bash
cd ~/icmp-server-final
sudo ./bin/icmp_server -i end0 -v
```

### 2. Lance le proxy HTTP sur Windows

```powershell
cd c:\icmp-browser\build-cef\Release
.\icmp_http_proxy.exe 192.168.1.40
```

Tu verras:
```
╔══════════════════════════════════════════════════════╗
║      ICMP HTTP Proxy - Standalone Version           ║
╠══════════════════════════════════════════════════════╣
║  Proxy is running!                                   ║
║  Configure your browser to use:                      ║
║  HTTP Proxy: 127.0.0.1:8888                         ║
╚══════════════════════════════════════════════════════╝
```

### 3. Configure ton navigateur

#### Chrome:
1. Settings → System → Open proxy settings
2. Manual proxy: `127.0.0.1` port `8888`

#### Firefox:
1. Settings → Network Settings → Manual proxy
2. HTTP Proxy: `127.0.0.1` port `8888`

#### Edge:
1. Settings → System → Open proxy settings
2. Manual proxy: `127.0.0.1` port `8888`

### 4. Navigate!

Ouvre ton navigateur et va sur:
- http://example.com (HTML pur, marche parfaitement)
- http://google.com (HTML charge, CSS/JS/images nécessitent plusieurs requêtes)

## 📊 Logs

Le proxy affiche:
- Requêtes reçues
- Chunks streamés (1, 2, 3... jusqu'au dernier)
- Bytes envoyés au navigateur
- Stats toutes les 10 secondes

## 🔧 Options

```
icmp_http_proxy.exe [server_ip] [proxy_port]

server_ip  : IP du serveur ICMP (défaut: 192.168.1.40)
proxy_port : Port d'écoute local (défaut: 8888)
```

Exemple:
```powershell
.\icmp_http_proxy.exe 192.168.1.50 9000
```

## ⚠️ Limitations actuelles

- Une requête HTTP à la fois (pas de parallélisme)
- Max 65KB par réponse HTTP
- Sous-ressources (CSS/JS/images) = requêtes séparées

Pour charger Google complètement, le navigateur fera automatiquement plusieurs requêtes:
1. `GET /` → HTML principal
2. `GET /style.css` → Feuille de style
3. `GET /logo.png` → Logo
4. etc.

## 🎉 Avantages

Comparé au navigateur CEF intégré:
- ✅ **Beaucoup plus simple** - juste un proxy
- ✅ **Marche avec n'importe quel navigateur** déjà installé
- ✅ **Pas de compilation CEF compliquée**
- ✅ **Réutilise le code qui marche** (streaming chunks)
- ✅ **Debug facile** - logs clairs dans la console

## 🐛 Debug

Si ça ne marche pas:
1. Vérifie que le serveur tourne: `ps aux | grep icmp_server`
2. Vérifie l'IP: `ping 192.168.1.40`
3. Check les logs du proxy pour voir le handshake
4. Test avec `curl --proxy http://127.0.0.1:8888 http://example.com`

## 📝 Architecture

```
Navigateur (Chrome/Firefox/Edge)
    ↓ HTTP via proxy 127.0.0.1:8888
Proxy HTTP (icmp_http_proxy.exe)
    ↓ Encapsulation ICMP + AES-GCM
    ↓ Streaming chunks (1200 bytes)
Serveur Orange Pi (192.168.1.40)
    ↓ Fetch HTTP/HTTPS
Internet
```

Bon surf! 🌐
