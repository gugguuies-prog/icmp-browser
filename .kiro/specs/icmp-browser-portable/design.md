# Design Document - ICMP Browser Portable

## 1. Introduction

Ce document présente le design technique du navigateur web portable Windows qui utilise le tunneling ICMP pour communiquer avec un serveur proxy, permettant de contourner les portails captifs sans droits administrateur.

**Validation technique** : Le prototype `test_icmp_dll.exe` a confirmé que l'API Windows `IcmpSendEcho()` fonctionne sans privilèges administrateur et permet de transporter des payloads personnalisés.

## 2. Architecture Système

### 2.1 Vue d'ensemble

```
┌─────────────────────────────────────────────────────────────┐
│                    ICMP BROWSER CLIENT                       │
│                      (Windows 10/11)                         │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐  │
│  │   WebView2   │◄───│ HTTP Proxy   │◄───│ ICMP Engine  │  │
│  │   Renderer   │    │    Engine    │    │ (Icmp.dll)   │  │
│  └──────────────┘    └──────────────┘    └──────────────┘  │
│         │                    │                    │          │
│         └────────────────────┴────────────────────┘          │
│                              │                               │
└──────────────────────────────┼───────────────────────────────┘
                               │ ICMP Echo Request/Reply
                               │ (Payload: Encrypted IP packets)
                               │
                     ┌─────────▼──────────┐
                     │ Network Interface  │
                     │  (Restricted WiFi) │
                     └─────────┬──────────┘
                               │
┌──────────────────────────────▼───────────────────────────────┐
│                   ICMP PROXY SERVER                          │
│                  (Linux/Armbian + root)                      │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐  │
│  │  Raw Socket  │───►│ Decapsulator │───►│  TUN Device  │  │
│  │ ICMP Receiver│    │  + Decrypt   │    │    (tun0)    │  │
│  └──────────────┘    └──────────────┘    └──────────────┘  │
│         ▲                                        │           │
│         │            ┌──────────────┐            │           │
│         └────────────┤ Encapsulator │◄───────────┘           │
│                      │  + Encrypt   │                        │
│                      └──────────────┘                        │
│                             │                                │
└─────────────────────────────┼────────────────────────────────┘
                              │
                    ┌─────────▼──────────┐
                    │ Internet (NAT/IP   │
                    │   Masquerading)    │
                    └────────────────────┘
```

### 2.2 Composants Client (Windows)

#### 2.2.1 WebView2 Renderer
- **Technologie** : Microsoft Edge WebView2 (Chromium)
- **Responsabilités** :
  - Rendu HTML/CSS/JavaScript
  - Génération de requêtes HTTP/HTTPS
  - Gestion des cookies et sessions
- **Interface** : WebView2 COM API

#### 2.2.2 HTTP Proxy Engine
- **Technologie** : Custom C/C++ proxy local
- **Port** : 127.0.0.1:8888 (local proxy)
- **Responsabilités** :
  - Intercepter toutes les requêtes HTTP/HTTPS de WebView2
  - Parser les requêtes HTTP en paquets IP
  - Transmettre au ICMP Engine
  - Recevoir les réponses IP et les reconstituer en HTTP
- **Protocole** : HTTP/1.1 et HTTP/2

#### 2.2.3 ICMP Engine (Client)
- **API Windows** : `Icmp.dll` (IcmpCreateFile, IcmpSendEcho, IcmpSendEcho2)
- **Responsabilités** :
  - Encapsuler les paquets IP dans ICMP Echo Request
  - Chiffrer les payloads (AES-256-GCM)
  - Envoyer via IcmpSendEcho()
  - Recevoir ICMP Echo Reply via IcmpSendEcho2()
  - Déchiffrer et décapsuler les réponses
- **Thread Model** : Multi-threaded (send + receive threads)

### 2.3 Composants Serveur (Linux/Armbian)

#### 2.3.1 Raw Socket ICMP Receiver
- **Technologie** : Raw sockets POSIX (IPPROTO_ICMP)
- **Privilèges** : Nécessite root
- **Responsabilités** :
  - Écouter tous les paquets ICMP Echo Request
  - Filtrer par ID de session client
  - Transmettre au Decapsulator

#### 2.3.2 Decapsulator + Decrypt
- **Responsabilités** :
  - Extraire la payload des paquets ICMP
  - Déchiffrer avec AES-256-GCM
  - Reconstruire les paquets IP fragmentés
  - Injecter dans l'interface TUN

#### 2.3.3 TUN Device (tun0)
- **Driver** : Linux TUN/TAP
- **Configuration** :
  - IP client virtuelle : 10.0.1.2/24
  - IP serveur : 10.0.1.1/24
- **Responsabilités** :
  - Interface réseau virtuelle pour injection/extraction IP
  - Routage via iptables NAT

#### 2.3.4 Encapsulator + Encrypt
- **Responsabilités** :
  - Lire les paquets IP de réponse depuis tun0
  - Chiffrer avec AES-256-GCM
  - Encapsuler dans ICMP Echo Reply
  - Envoyer via raw socket vers le client

## 3. Protocole de Communication

### 3.1 Format du Paquet ICMP Tunnelisé

```
+-------------------------------------------+
| IP Header (20 bytes)                      |
|  - Source: Client Public IP               |
|  - Dest: Server Public IP                 |
|  - Protocol: ICMP (1)                     |
+-------------------------------------------+
| ICMP Header (8 bytes)                     |
|  - Type: 8 (Echo Request) ou 0 (Reply)    |
|  - Code: 0                                |
|  - Checksum: Calculated                   |
|  - ID: Session ID (16-bit)                |
|  - Sequence: Packet counter (16-bit)      |
+-------------------------------------------+
| Encrypted Payload (variable, max 1472)    |
|  +-------------------------------------+  |
|  | IV (12 bytes) - AES-GCM Nonce      |  |
|  +-------------------------------------+  |
|  | Encrypted IP Packet (variable)     |  |
|  |  - Original IP Header              |  |
|  |  - Original Transport Layer (TCP)  |  |
|  |  - Original Data (HTTP, etc.)      |  |
|  +-------------------------------------+  |
|  | Auth Tag (16 bytes) - GCM Tag      |  |
|  +-------------------------------------+  |
+-------------------------------------------+
```

### 3.2 Handshake et Établissement de Session

#### Phase 1: Échange de clés Diffie-Hellman

```
Client                                    Server
  │                                          │
  │  ICMP Echo Request (Type=8, Seq=0)       │
  ├─────────────────────────────────────────►│
  │  Payload: DH_PUBLIC_KEY_CLIENT           │
  │           (2048-bit)                     │
  │                                          │
  │  ICMP Echo Reply (Type=0, Seq=0)         │
  │◄─────────────────────────────────────────┤
  │  Payload: DH_PUBLIC_KEY_SERVER           │
  │           (2048-bit)                     │
  │                                          │
  ├─ Compute Shared Secret ─────────────────┤
  │  Shared_Secret = DH(client_priv,         │
  │                     server_pub)          │
  │                                          │
  ├─ Derive Session Key ────────────────────┤
  │  Session_Key = HKDF-SHA256(              │
  │      Shared_Secret, "ICMP_TUNNEL_v1")    │
  │                                          │
```

#### Phase 2: Transmission de données

```
Client                                    Server
  │                                          │
  │  ICMP Echo Request (Seq=1..N)            │
  ├─────────────────────────────────────────►│
  │  Payload: AES-256-GCM(IP_Packet)         │
  │           using Session_Key              │
  │                                          │
  │  ICMP Echo Reply (Seq=1..N)              │
  │◄─────────────────────────────────────────┤
  │  Payload: AES-256-GCM(IP_Response)       │
  │                                          │
```

### 3.3 Gestion de la Fragmentation

Quand un paquet IP > 1432 bytes (MTU - 28 bytes overhead - 12 IV):

```
Original IP Packet: 3000 bytes
│
├─► Fragment 1 (Seq=N, Flags=0x01): 1432 bytes
├─► Fragment 2 (Seq=N+1, Flags=0x01): 1432 bytes  
└─► Fragment 3 (Seq=N+2, Flags=0x00): 136 bytes (last)

Flags: 0x01 = More fragments
       0x00 = Last fragment
```

**Réassemblage** :
- Le récepteur maintient un buffer par Session ID
- Timeout de réassemblage : 10 secondes
- Si timeout, supprimer tous les fragments partiels

## 4. Structures de Données

### 4.1 Client (C/C++)

```c
// Session ICMP
typedef struct {
    HANDLE hIcmp;                    // Handle Icmp.dll
    uint16_t session_id;             // ID unique de session
    uint16_t sequence_counter;       // Compteur de séquence
    uint8_t session_key[32];         // Clé AES-256
    struct sockaddr_in server_addr;  // Adresse du serveur
    pthread_mutex_t send_lock;       // Lock pour envoi
    pthread_mutex_t recv_lock;       // Lock pour réception
} icmp_session_t;

// Paquet ICMP encapsulé
typedef struct {
    uint8_t type;                    // 8 (Echo Request) ou 0 (Reply)
    uint16_t id;                     // Session ID
    uint16_t sequence;               // Numéro de séquence
    uint8_t *payload;                // Données chiffrées
    size_t payload_size;             // Taille du payload
} icmp_packet_t;

// Fragment de paquet IP
typedef struct {
    uint16_t session_id;
    uint16_t sequence_base;
    uint8_t flags;                   // 0x01 = more, 0x00 = last
    uint8_t *data;
    size_t data_size;
    time_t timestamp;
} ip_fragment_t;

// File d'attente de paquets
typedef struct packet_queue {
    icmp_packet_t packet;
    struct packet_queue *next;
} packet_queue_t;
```

### 4.2 Serveur (C)

```c
// Session client
typedef struct {
    uint32_t client_ip;              // IP publique du client
    uint16_t session_id;             // ID de session
    uint8_t session_key[32];         // Clé AES-256
    time_t last_activity;            // Timestamp dernière activité
    int tun_fd;                      // File descriptor TUN
    pthread_t send_thread;           // Thread d'envoi
    pthread_t recv_thread;           // Thread de réception
} client_session_t;

// Table de sessions (hash map)
typedef struct {
    client_session_t sessions[MAX_CLIENTS];
    size_t count;
    pthread_rwlock_t lock;
} session_table_t;
```

## 5. Algorithmes Clés

### 5.1 Encapsulation ICMP (Client)

```python
function encapsulate_ip_packet(ip_packet, session):
    # Fragmenter si nécessaire
    if len(ip_packet) > MAX_PAYLOAD_SIZE:
        fragments = fragment_ip_packet(ip_packet, MAX_PAYLOAD_SIZE)
        for i, frag in enumerate(fragments):
            is_last = (i == len(fragments) - 1)
            send_fragment(frag, session, is_last)
    else:
        # Paquet complet, pas de fragmentation
        send_single_packet(ip_packet, session)

function send_single_packet(ip_packet, session):
    # Générer IV aléatoire (96 bits)
    iv = random_bytes(12)
    
    # Chiffrer avec AES-256-GCM
    ciphertext, auth_tag = aes_gcm_encrypt(
        key=session.session_key,
        iv=iv,
        plaintext=ip_packet,
        associated_data=struct.pack('>HH', session.id, session.sequence)
    )
    
    # Construire le payload ICMP
    payload = iv + ciphertext + auth_tag
    
    # Créer le paquet ICMP
    icmp_packet = create_icmp_packet(
        type=ICMP_ECHO_REQUEST,
        id=session.session_id,
        sequence=session.sequence_counter,
        payload=payload
    )
    
    # Envoyer via IcmpSendEcho
    result = IcmpSendEcho(
        session.hIcmp,
        session.server_addr.sin_addr,
        payload,
        len(payload),
        NULL,  # Options IP
        reply_buffer,
        reply_buffer_size,
        5000   # Timeout 5s
    )
    
    session.sequence_counter += 1
    
    return result > 0
```

### 5.2 Décapsulation ICMP (Serveur)

```python
function decapsulate_icmp_packet(raw_packet, session_table):
    # Parser le paquet ICMP brut
    ip_header = parse_ip_header(raw_packet[0:20])
    icmp_header = parse_icmp_header(raw_packet[20:28])
    payload = raw_packet[28:]
    
    # Vérifier le type ICMP
    if icmp_header.type != ICMP_ECHO_REQUEST:
        return None
    
    # Trouver la session correspondante
    session = session_table.find(icmp_header.id)
    if not session:
        # Nouvelle session, initialiser
        session = create_new_session(ip_header.src_addr, icmp_header.id)
        session_table.add(session)
    
    # Extraire IV, ciphertext, auth_tag
    iv = payload[0:12]
    ciphertext = payload[12:-16]
    auth_tag = payload[-16:]
    
    # Déchiffrer avec AES-256-GCM
    try:
        ip_packet = aes_gcm_decrypt(
            key=session.session_key,
            iv=iv,
            ciphertext=ciphertext,
            auth_tag=auth_tag,
            associated_data=struct.pack('>HH', icmp_header.id, icmp_header.sequence)
        )
    except AuthenticationError:
        log_error("GCM authentication failed")
        return None
    
    # Gérer la fragmentation si nécessaire
    if is_fragmented(payload):
        ip_packet = reassemble_fragments(ip_packet, session)
        if not ip_packet:
            return None  # Attendre plus de fragments
    
    # Injecter dans l'interface TUN
    write(session.tun_fd, ip_packet, len(ip_packet))
    
    # Mettre à jour l'activité
    session.last_activity = time.now()
    
    return ip_packet
```

### 5.3 Gestion du Timeout et Retransmission (Client)

```python
function reliable_send(ip_packet, session, max_retries=3):
    for attempt in range(max_retries):
        # Envoyer le paquet
        success = send_single_packet(ip_packet, session)
        
        if success:
            # Attendre l'acquittement (Echo Reply)
            ack = wait_for_ack(session, timeout=5.0)
            if ack:
                return True
        
        # Échec ou timeout, réessayer avec backoff exponentiel
        delay = min(1.0 * (2 ** attempt), 10.0)
        sleep(delay)
    
    # Toutes les tentatives ont échoué
    log_error(f"Failed to send packet after {max_retries} retries")
    return False

function wait_for_ack(session, timeout):
    start_time = time.now()
    while time.now() - start_time < timeout:
        # Vérifier la file de réception
        reply = session.recv_queue.pop_timeout(0.1)
        if reply and reply.sequence == session.sequence_counter - 1:
            return reply
    return None
```

## 6. Intégration WebView2

### 6.1 Configuration du Proxy

```cpp
// Configuration WebView2 pour utiliser le proxy local
CoreWebView2EnvironmentOptions options;
options.SetAdditionalBrowserArguments(
    L"--proxy-server=http://127.0.0.1:8888 "
    L"--disable-web-security "  // Pour tests uniquement
    L"--ignore-certificate-errors"
);

// Créer l'environnement WebView2
CreateCoreWebView2EnvironmentWithOptions(
    nullptr,  // browserExecutableFolder
    userDataFolder,
    &options,
    &webViewEnvironmentCallback
);
```

### 6.2 Interception HTTP

```cpp
// Proxy HTTP local (port 8888)
class HttpProxyServer {
public:
    void Start(int port) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        
        listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = htons(port);
        
        bind(listen_socket, (sockaddr*)&addr, sizeof(addr));
        listen(listen_socket, SOMAXCONN);
        
        // Boucle d'acceptation
        while (running) {
            SOCKET client_socket = accept(listen_socket, NULL, NULL);
            std::thread(&HttpProxyServer::HandleClient, this, client_socket).detach();
        }
    }
    
    void HandleClient(SOCKET client_socket) {
        char buffer[8192];
        int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        
        // Parser la requête HTTP
        HttpRequest request = ParseHttpRequest(buffer, bytes_received);
        
        // Convertir en paquets IP et envoyer via ICMP tunnel
        std::vector<uint8_t> ip_packets = ConvertHttpToIp(request);
        for (auto& packet : ip_packets) {
            icmp_engine->SendPacket(packet);
        }
        
        // Attendre la réponse via ICMP tunnel
        std::vector<uint8_t> ip_response = icmp_engine->WaitForResponse(request.id);
        
        // Convertir la réponse IP en HTTP
        HttpResponse response = ConvertIpToHttp(ip_response);
        
        // Renvoyer au client (WebView2)
        send(client_socket, response.data(), response.size(), 0);
        closesocket(client_socket);
    }
    
private:
    SOCKET listen_socket;
    bool running = true;
    IcmpEngine* icmp_engine;
};
```

## 7. Serveur Armbian - Adaptations

### 7.1 Spécificités Armbian

- **Architecture** : ARM (32-bit ou 64-bit)
- **Kernel** : Linux 6.x avec support TUN/TAP
- **Dépendances** :
  ```bash
  sudo apt-get install build-essential iptables iproute2
  ```

### 7.2 Configuration Réseau (server.sh pour Armbian)

```bash
#!/bin/bash
# Configuration du serveur ICMP pour Armbian

# Créer l'interface TUN
ip tuntap add mode tun tun0
ip addr add 10.0.1.1/24 dev tun0
ip link set tun0 up

# Activer le forwarding IP
echo 1 > /proc/sys/net/ipv4/ip_forward

# Désactiver la réponse automatique aux ICMP Echo Request
echo 1 > /proc/sys/net/ipv4/icmp_echo_ignore_all

# Configurer NAT avec iptables
iptables -t nat -A POSTROUTING -s 10.0.1.0/24 -o eth0 -j MASQUERADE
iptables -A FORWARD -i tun0 -o eth0 -j ACCEPT
iptables -A FORWARD -i eth0 -o tun0 -m state --state RELATED,ESTABLISHED -j ACCEPT

echo "Serveur ICMP prêt sur Armbian"
```

### 7.3 Compilation Cross-Platform

```makefile
# Makefile pour serveur (Linux/Armbian)
CC=gcc
CFLAGS=-Wall -O2 -pthread
LDFLAGS=-lpthread -lcrypto

# Détecter l'architecture
ARCH := $(shell uname -m)
ifeq ($(ARCH),armv7l)
    CFLAGS += -march=armv7-a -mfpu=neon
else ifeq ($(ARCH),aarch64)
    CFLAGS += -march=armv8-a
endif

all: icmp_server

icmp_server: server.o icmp_handler.o crypto.o tunnel.o
	$(CC) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o icmp_server
```

## 8. Sécurité

### 8.1 Chiffrement AES-256-GCM

- **Algorithme** : AES-256 en mode GCM (Galois/Counter Mode)
- **Taille de clé** : 256 bits (32 bytes)
- **IV** : 96 bits (12 bytes), généré aléatoirement pour chaque paquet
- **Auth Tag** : 128 bits (16 bytes) pour l'authentification

**Avantages de GCM** :
- Chiffrement + authentification en une seule passe
- Performances élevées (hardware acceleration possible)
- Protection contre tampering et replay attacks

### 8.2 Échange de Clés Diffie-Hellman

```python
# Paramètres DH (2048-bit)
P = 0xFFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB9ED529077096966D670C354E4ABC9804F1746C08CA18217C32905E462E36CE3BE39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9DE2BCBF6955817183995497CEA956AE515D2261898FA051015728E5A8AACAA68FFFFFFFFFFFFFFFF

G = 2

# Client génère sa clé privée
client_private = random_integer(2, P-2)
client_public = pow(G, client_private, P)

# Serveur génère sa clé privée
server_private = random_integer(2, P-2)
server_public = pow(G, server_private, P)

# Échange des clés publiques via ICMP

# Calcul du secret partagé
shared_secret = pow(server_public, client_private, P)  # Côté client
shared_secret = pow(client_public, server_private, P)  # Côté serveur (même valeur)

# Dérivation de la clé de session avec HKDF
session_key = HKDF(
    algorithm=SHA256,
    length=32,
    salt=b"ICMP_TUNNEL_SALT_v1",
    info=b"SESSION_KEY",
    input_key_material=shared_secret.to_bytes(256, 'big')
)
```

## 9. Performance et Optimisations

### 9.1 Threading Model

**Client** :
- Thread principal : UI (WebView2)
- Thread proxy HTTP : Accepte les connexions du navigateur
- Thread ICMP Send : Envoie les paquets ICMP
- Thread ICMP Receive : Reçoit les paquets ICMP (IcmpSendEcho2 avec callback)

**Serveur** :
- Thread principal : Écoute raw socket ICMP
- Thread par client : Gère tun0 read/write pour chaque session
- Thread de nettoyage : Supprime les sessions inactives

### 9.2 Files d'Attente et Priorités

```c
typedef enum {
    PRIORITY_HIGH = 0,    // DNS, handshakes
    PRIORITY_NORMAL = 1,  // Requêtes HTTP normales
    PRIORITY_LOW = 2      // Images, ressources statiques
} packet_priority_t;

typedef struct {
    packet_queue_t *queues[3];  // Une queue par priorité
    pthread_mutex_t lock;
    pthread_cond_t cond;
    size_t total_size;
    size_t max_size;  // 1000 paquets max
} priority_queue_t;

// Enqueue avec priorité
void pq_enqueue(priority_queue_t *pq, icmp_packet_t *packet, packet_priority_t priority) {
    pthread_mutex_lock(&pq->lock);
    
    // Supprimer les vieux paquets si pleine
    if (pq->total_size >= pq->max_size) {
        // Supprimer le plus vieux paquet de priorité LOW
        dequeue_oldest(&pq->queues[PRIORITY_LOW]);
    }
    
    // Ajouter à la queue appropriée
    queue_push(pq->queues[priority], packet);
    pq->total_size++;
    
    pthread_cond_signal(&pq->cond);
    pthread_mutex_unlock(&pq->lock);
}

// Dequeue avec priorité
icmp_packet_t* pq_dequeue(priority_queue_t *pq) {
    pthread_mutex_lock(&pq->lock);
    
    // Attendre qu'un paquet soit disponible
    while (pq->total_size == 0) {
        pthread_cond_wait(&pq->cond, &pq->lock);
    }
    
    // Déqueue par ordre de priorité
    icmp_packet_t *packet = NULL;
    for (int i = PRIORITY_HIGH; i <= PRIORITY_LOW; i++) {
        if (!queue_is_empty(pq->queues[i])) {
            packet = queue_pop(pq->queues[i]);
            pq->total_size--;
            break;
        }
    }
    
    pthread_mutex_unlock(&pq->lock);
    return packet;
}
```

### 9.3 Compression des Payloads

```c
#include <zlib.h>

// Compresser avant chiffrement
size_t compress_payload(uint8_t *input, size_t input_size, 
                        uint8_t *output, size_t output_max_size) {
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    
    deflateInit(&stream, Z_BEST_COMPRESSION);
    
    stream.avail_in = input_size;
    stream.next_in = input;
    stream.avail_out = output_max_size;
    stream.next_out = output;
    
    deflate(&stream, Z_FINISH);
    size_t compressed_size = output_max_size - stream.avail_out;
    
    deflateEnd(&stream);
    return compressed_size;
}

// Décompresser après déchiffrement
size_t decompress_payload(uint8_t *input, size_t input_size,
                          uint8_t *output, size_t output_max_size) {
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    
    inflateInit(&stream);
    
    stream.avail_in = input_size;
    stream.next_in = input;
    stream.avail_out = output_max_size;
    stream.next_out = output;
    
    inflate(&stream, Z_FINISH);
    size_t decompressed_size = output_max_size - stream.avail_out;
    
    inflateEnd(&stream);
    return decompressed_size;
}
```

## 10. Interface Utilisateur (Client)

### 10.1 Fenêtre Principale

```
┌─────────────────────────────────────────────────────────────┐
│  [ICMP Browser]                                    [_][□][X] │
├─────────────────────────────────────────────────────────────┤
│  [◄] [►] [⟳] [🏠]  [https://example.com            ] [🔍]  │
│                                                     [⚙]      │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│                                                               │
│                    WebView2 Content Area                      │
│                                                               │
│                                                               │
│                                                               │
├─────────────────────────────────────────────────────────────┤
│ Status: ● Connected to 192.168.1.100 | ↑ 125 KB/s ↓ 450 KB/s│
└─────────────────────────────────────────────────────────────┘
```

### 10.2 Menu Paramètres

```
┌───────────────────────────────────┐
│  Paramètres ICMP Browser          │
├───────────────────────────────────┤
│  Serveur Proxy:                   │
│  [192.168.1.100        ] [Test]   │
│                                    │
│  Débit maximal (paquets/s):       │
│  [100                  ] [▼]      │
│                                    │
│  ☑ Compression zlib               │
│  ☑ Chiffrement AES-256            │
│  ☐ Logs détaillés                 │
│                                    │
│  [Afficher les Logs] [Exporter]   │
│                                    │
│          [Annuler]  [Enregistrer] │
└───────────────────────────────────┘
```

## 11. Tests et Validation

### 11.1 Tests Unitaires

- **Test du chiffrement AES-GCM** : Vérifier round-trip encryption/decryption
- **Test de fragmentation** : Paquets IP de différentes tailles (64, 1500, 3000, 9000 bytes)
- **Test de Diffie-Hellman** : Vérifier que client et serveur obtiennent la même clé
- **Test de compression** : Vérifier round-trip compress/decompress

### 11.2 Tests d'Intégration

- **Test client-serveur local** : Client et serveur sur la même machine
- **Test via WiFi restreint** : Client sur réseau captif, serveur sur Internet
- **Test de charge** : 100 requêtes HTTP simultanées
- **Test de reconnexion** : Perte de connexion réseau puis rétablissement

### 11.3 Tests de Performance

- **Latence** : Mesurer RTT moyen (devrait être < 100ms en plus de la latence réseau)
- **Débit** : Mesurer le throughput (objectif: > 1 Mbps sur connexion moyenne)
- **Utilisation CPU** : < 20% sur client, < 10% sur serveur (idle)
- **Utilisation mémoire** : < 100 MB sur client, < 50 MB sur serveur

### 11.4 Tests de Sécurité

- **Test de Wireshark** : Vérifier que les payloads sont chiffrés
- **Test de replay attack** : Envoyer un vieux paquet, vérifier qu'il est rejeté
- **Test de tampering** : Modifier un paquet chiffré, vérifier échec d'authentification GCM

## 12. Déploiement

### 12.1 Client Windows

**Distribution** :
- Un seul fichier `ICMPBrowser.exe` (portable)
- Fichier de configuration `config.ini` (généré au premier lancement)
- WebView2 Runtime (téléchargé automatiquement si absent)

**Structure du dossier** :
```
ICMPBrowser/
├── ICMPBrowser.exe
├── config.ini
├── icmp_browser.log
└── webview2_cache/ (créé automatiquement)
```

### 12.2 Serveur Armbian

**Installation** :
```bash
# Télécharger les sources
git clone https://github.com/your-repo/icmp-browser-server.git
cd icmp-browser-server

# Compiler
make

# Installer
sudo make install

# Configurer
sudo nano /etc/icmp-server.conf

# Démarrer le service
sudo systemctl start icmp-server
sudo systemctl enable icmp-server
```

**Configuration serveur (`/etc/icmp-server.conf`)** :
```ini
[server]
listen_interface=eth0
tun_ip=10.0.1.1
tun_netmask=255.255.255.0
max_clients=100
session_timeout=300

[logging]
log_level=info
log_file=/var/log/icmp-server.log
```

## 13. Conclusion

Ce design fournit une architecture complète pour implémenter le navigateur ICMP portable. Les prochaines étapes consistent à :

1. Implémenter les composants client (ICMP Engine, HTTP Proxy, UI)
2. Implémenter le serveur (raw socket, TUN/TAP, NAT)
3. Tester l'intégration client-serveur
4. Optimiser les performances
5. Déployer et valider sur des environnements réels

**Note importante** : Ce système est conçu pour contourner les restrictions réseau légitimes. Son utilisation doit être conforme aux lois locales et aux politiques réseau de l'organisation.
