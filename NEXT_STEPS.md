# 🎯 PROCHAINES ÉTAPES - ICMP Browser

**Date:** 2024-11-17  
**Statut:** Task 4.1 en cours (70% complété)

---

## 📊 Situation Actuelle

✅ **13/20 tâches terminées (65%)**
- Phase 0: Validation ✅
- Phase 1: Fondations (crypto + ICMP) ✅
- Phase 2: Protocole ✅
- Phase 3: TUN/TAP ✅
- Phase 5: Optimisations ✅

🔄 **Phase 4 EN COURS (20h total)**
- Task 4.1: Proxy HTTP (8h) - 70% fait ⬅️ **TU ES ICI**
- Task 4.2: WebView2 (6h) - À faire
- Task 4.3: UI (6h) - À faire

⏸️ **Phase 6 À VENIR (20h)**
- Tests end-to-end avec serveur Armbian

---

## 🔧 Task 4.1: Proxy HTTP Local (70% COMPLÉTÉ)

### ✅ Ce qui est fait:

1. **Structure complète** (`src/client/http_proxy.h`)
   - `http_proxy_t` - Serveur proxy
   - `http_request_t` - Requête HTTP
   - `http_response_t` - Réponse HTTP
   - `http_client_t` - Connexion client

2. **Implémentation de base** (`src/client/http_proxy.c` - 464 lignes)
   - Serveur TCP sur 127.0.0.1:8888
   - Accept thread pour nouvelles connexions
   - Client handler threads (32 max simultanés)
   - `http_parse_request()` - Parse GET/POST
   - `http_build_response()` - Construit réponses
   - Statistiques (requests, bytes)

3. **Tests unitaires** (`tests/test_http_proxy.c` - 274 lignes)
   - 7 tests créés
   - Compilation OK (warnings %zu à fixer)

### ⏸️ Ce qui reste (30%):

#### 1. Fixer warnings et tester (1h)

```bash
# Dans les tests, remplacer tous les %zu par %lu pour MinGW
# Fichier: tests/test_http_proxy.c lignes 159, 179, 201

# Tester
wsl make test-http-proxy-win

# Résultat attendu: 7/7 tests passent
```

#### 2. Intégrer avec ICMP tunnel (5h)

**Localisation:** `src/client/http_proxy.c`, fonction `client_handler_thread()` ligne ~260

**Code actuel (TODO):**
```c
// TODO: Parse and handle HTTP request
// TODO: Forward through ICMP tunnel
// TODO: Receive response from tunnel
// TODO: Send response to client

// For now, send a simple response
const char *response = "HTTP/1.1 200 OK...";
```

**Ce qu'il faut faire:**

```c
// 1. Parser la requête HTTP
http_request_t req;
if (http_parse_request(buffer, bytes_received, &req) != 0) {
    // Erreur de parsing
    break;
}

// 2. Construire paquet TCP/IP à partir de la requête HTTP
// Format: IP header + TCP header + HTTP request
uint8_t ip_packet[2048];
size_t ip_len = build_tcp_ip_packet(&req, ip_packet, sizeof(ip_packet));

// 3. Envoyer via ICMP tunnel
icmp_session_t *session = proxy->icmp_session;
icmp_send_packet(session, ip_packet, ip_len);

// 4. Attendre réponse IP du tunnel
uint8_t reply_packet[2048];
size_t reply_len;
icmp_wait_reply(session, reply_packet, &reply_len, 5000);

// 5. Extraire réponse HTTP du paquet IP
http_response_t resp;
parse_ip_to_http_response(reply_packet, reply_len, &resp);

// 6. Construire et envoyer réponse HTTP au client
uint8_t response_buffer[65536];
size_t response_len;
http_build_response(&resp, response_buffer, sizeof(response_buffer), &response_len);
send(client->socket_fd, response_buffer, response_len, 0);
```

**Fonctions helper à créer:**
- `build_tcp_ip_packet()` - Construit IP+TCP+HTTP
- `parse_ip_to_http_response()` - Extrait HTTP d'un paquet IP

#### 3. Tester avec curl (2h)

```bash
# Démarrer le proxy
./bin/icmp_proxy.exe --server <IP_ARMBIAN>

# Dans un autre terminal
curl --proxy http://127.0.0.1:8888 http://example.com

# Résultat attendu: Page HTML de example.com
```

---

## 🚀 Après Task 4.1 (Tasks 4.2 et 4.3)

### Task 4.2: WebView2 (6h)
- Créer fenêtre Win32
- Intégrer WebView2 Runtime
- Configurer proxy dans WebView2
- Barre d'adresse fonctionnelle

### Task 4.3: UI (6h)
- Toolbar (back, forward, reload, home)
- Status bar (connexion, débit)
- Dialog paramètres

---

## 📁 Fichiers Importants

**Proxy HTTP:**
- `src/client/http_proxy.h` - API (213 lignes)
- `src/client/http_proxy.c` - Implémentation (464 lignes)
- `tests/test_http_proxy.c` - Tests (274 lignes)

**ICMP Engine (pour intégration):**
- `src/client/icmp_engine.h` - API ICMP
- `src/client/icmp_engine.c` - Client Windows

**Progression:**
- `PROGRESS.md` - Statut détaillé ⭐
- `PROJECT_STATUS.md` - Vue d'ensemble
- `.kiro/specs/icmp-browser-portable/tasks.md` - Toutes les tâches

---

## 💡 Conseils pour le Prochain Agent

1. **Lire d'abord:**
   - `PROGRESS.md` section "POINT DE REPRISE"
   - `src/client/http_proxy.h` pour comprendre l'API
   - `src/client/icmp_engine.h` pour l'intégration

2. **Priorités:**
   - ⭐ Fixer warnings %zu → %lu (5 min)
   - ⭐ Tester les 7 tests unitaires (15 min)
   - ⭐⭐ Implémenter intégration ICMP (5h)
   - Tester avec curl (2h)

3. **Tests avant de continuer:**
   ```bash
   wsl make test-http-proxy-win  # 7 tests doivent passer
   ```

4. **Si bloqué:**
   - Tous les modules backend (crypto, ICMP, protocole) sont 100% fonctionnels
   - 87 tests unitaires passent à 100%
   - Le seul challenge est l'intégration HTTP ↔ ICMP

5. **Résultats attendus Task 4.1:**
   - Proxy écoute sur localhost:8888 ✅
   - Accepte connexions HTTP ✅
   - Parse requêtes ✅
   - Envoie via ICMP ⏸️ (à faire)
   - Reçoit réponses ⏸️ (à faire)
   - Répond au client ⏸️ (à faire)

---

## 🎯 Objectif Final Phase 4

**Produit fini:** Navigateur Windows portable utilisant ICMP pour tunneliser tout le trafic HTTP, SANS droits admin.

**Démonstration:**
1. Lancer serveur sur Armbian (utilisateur a un serveur prêt)
2. Lancer client Windows
3. Naviguer sur le web via WebView2
4. Tout le trafic passe par ICMP tunnel

**Temps restant estimé:**
- Task 4.1 finition: 6h
- Task 4.2: 6h
- Task 4.3: 6h
- **Total Phase 4: 18h**

Bon courage ! 💪
