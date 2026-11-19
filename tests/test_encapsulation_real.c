/**
 * test_encapsulation_real.c
 * 
 * Test d'encapsulation réelle : envoie une requête HTTP encapsulée
 * via ICMP à un serveur distant et décode la réponse
 * 
 * Usage: test_encapsulation_real.exe <server_ip> [url]
 *   server_ip : Adresse IP du serveur ICMP (ex: 192.168.1.100)
 *   url       : URL à requêter (défaut: http://example.com)
 * 
 * Example: test_encapsulation_real.exe 192.168.1.100 http://example.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/common/protocol.h"
#include "../src/common/handshake.h"
#include "../src/crypto/aes_gcm.h"
#include "../src/crypto/diffie_hellman.h"
#include "../src/client/icmp_engine.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#endif

// Couleurs pour le terminal Windows
#define COLOR_RESET   ""
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"

/**
 * Calculer le checksum TCP/IP
 */
uint16_t calculate_checksum(uint16_t *addr, int len) {
    int nleft = len;
    uint32_t sum = 0;
    uint16_t *w = addr;
    uint16_t answer = 0;

    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1) {
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;

    return answer;
}

/**
 * Créer un paquet HTTP GET encapsulé dans TCP/IP
 */
size_t create_http_get_packet(const char *host, const char *path, 
                               const char *src_ip, const char *dst_ip,
                               uint8_t *buffer, size_t max_size) {
#ifdef _MSC_VER
#pragma pack(push, 1)
    struct iphdr {
        uint8_t version_ihl;      // Version (4 bits) + IHL (4 bits)
        uint8_t tos;              // Type of service
        uint16_t tot_len;         // Total length
        uint16_t id;              // Identification
        uint16_t frag_off;        // Fragment offset
        uint8_t ttl;              // Time to live
        uint8_t protocol;         // Protocol
        uint16_t check;           // Checksum
        uint32_t saddr;           // Source address
        uint32_t daddr;           // Destination address
    };

    struct tcphdr {
        uint16_t source;          // Source port
        uint16_t dest;            // Destination port
        uint32_t seq;             // Sequence number
        uint32_t ack_seq;         // Acknowledgment number
        uint16_t res1_doff_flags; // Data offset + flags
        uint16_t window;          // Window size
        uint16_t check;           // Checksum
        uint16_t urg_ptr;         // Urgent pointer
    };
#pragma pack(pop)
#else
    struct iphdr {
        uint8_t version_ihl;      // Version (4 bits) + IHL (4 bits)
        uint8_t tos;              // Type of service
        uint16_t tot_len;         // Total length
        uint16_t id;              // Identification
        uint16_t frag_off;        // Fragment offset
        uint8_t ttl;              // Time to live
        uint8_t protocol;         // Protocol
        uint16_t check;           // Checksum
        uint32_t saddr;           // Source address
        uint32_t daddr;           // Destination address
    } __attribute__((packed));

    struct tcphdr {
        uint16_t source;          // Source port
        uint16_t dest;            // Destination port
        uint32_t seq;             // Sequence number
        uint32_t ack_seq;         // Acknowledgment number
        uint16_t res1_doff_flags; // Data offset + flags
        uint16_t window;          // Window size
        uint16_t check;           // Checksum
        uint16_t urg_ptr;         // Urgent pointer
    } __attribute__((packed));
#endif

    // Construire la requête HTTP
    char http_request[1024];
    snprintf(http_request, sizeof(http_request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: ICMP-Browser/1.0\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, host);

    size_t http_len = strlen(http_request);
    size_t total_len = sizeof(struct iphdr) + sizeof(struct tcphdr) + http_len;

    if (total_len > max_size) {
        return 0;
    }

    // IP Header
    struct iphdr *ip = (struct iphdr *)buffer;
    memset(ip, 0, sizeof(struct iphdr));
    ip->version_ihl = 0x45;           // Version 4, IHL 5 (20 bytes)
    ip->tos = 0;
    ip->tot_len = htons(total_len);
    ip->id = htons(rand() % 65535);
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->protocol = 6;                 // TCP
    ip->check = 0;
    ip->saddr = inet_addr(src_ip);
    ip->daddr = inet_addr(dst_ip);
    ip->check = calculate_checksum((uint16_t *)ip, sizeof(struct iphdr));

    // TCP Header
    struct tcphdr *tcp = (struct tcphdr *)(buffer + sizeof(struct iphdr));
    memset(tcp, 0, sizeof(struct tcphdr));
    tcp->source = htons(12345);       // Port source aléatoire
    tcp->dest = htons(80);            // HTTP port
    tcp->seq = htonl(rand());
    tcp->ack_seq = 0;
    tcp->res1_doff_flags = htons(0x5002);  // Data offset=5, SYN flag
    tcp->window = htons(65535);
    tcp->check = 0;
    tcp->urg_ptr = 0;

    // HTTP Data
    memcpy(buffer + sizeof(struct iphdr) + sizeof(struct tcphdr), 
           http_request, http_len);

    // Recalculer le checksum TCP (simplifié - pas de pseudo-header pour le test)
    tcp->check = calculate_checksum((uint16_t *)tcp, 
                                     sizeof(struct tcphdr) + http_len);

    printf("%s[+]%s Paquet HTTP GET créé (%zu bytes)\n", 
           COLOR_GREEN, COLOR_RESET, total_len);
    printf("    Requête: GET %s from %s\n", path, host);
    printf("    IP: %s -> %s\n", src_ip, dst_ip);
    printf("    TCP: port %d -> port 80\n", 12345);

    return total_len;
}

/**
 * Afficher les données en hexadécimal
 */
void print_hex(const char *label, const uint8_t *data, size_t len, size_t max_print) {
    printf("%s (%zu bytes): ", label, len);
    size_t print_len = (len < max_print) ? len : max_print;
    for (size_t i = 0; i < print_len; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n    ");
    }
    if (len > max_print) printf("... (%zu more bytes)", len - max_print);
    printf("\n");
}

/**
 * Tester la connectivité ICMP basique
 */
int test_connectivity(icmp_session_t *session) {
    printf("\n%s[*]%s Test de connectivité ICMP...\n", COLOR_CYAN, COLOR_RESET);
    
    uint32_t rtt;
    int result = icmp_test_connectivity(session, &rtt);
    
    if (result == ICMP_SUCCESS) {
        printf("%s[✓]%s Serveur accessible ! RTT: %u ms\n", 
               COLOR_GREEN, COLOR_RESET, rtt);
        return 0;
    } else {
        printf("%s[✗]%s Serveur non accessible: %s\n", 
               COLOR_RED, COLOR_RESET, icmp_error_string(result));
        return -1;
    }
}

/**
 * Effectuer le handshake Diffie-Hellman
 */
int perform_handshake(icmp_session_t *icmp_session, handshake_context_t *hs_ctx) {
    printf("\n%s[*]%s Démarrage du handshake Diffie-Hellman...\n", 
           COLOR_CYAN, COLOR_RESET);
    
    // Initialiser le contexte de handshake
    uint16_t session_id = icmp_session->session_id;
    int result = handshake_init(hs_ctx, session_id);
    if (result != HANDSHAKE_SUCCESS) {
        printf("%s[✗]%s Échec initialisation handshake: %s\n", 
               COLOR_RED, COLOR_RESET, handshake_error_string(result));
        return -1;
    }
    
    // Générer la requête de handshake (client)
    proto_packet_t request;
    result = handshake_client_start(hs_ctx, &request);
    if (result != HANDSHAKE_SUCCESS) {
        printf("%s[✗]%s Échec génération requête handshake: %s\n", 
               COLOR_RED, COLOR_RESET, handshake_error_string(result));
        return -1;
    }
    
    printf("%s[+]%s Clé publique DH générée (%zu bytes)\n", 
           COLOR_GREEN, COLOR_RESET, request.payload_size);
    
    // Construire le paquet ICMP pour l'envoi
    // Format: [Proto Header (8 bytes)] [Payload (DH public key 256 bytes)]
    uint8_t icmp_payload[8 + 256];
    memcpy(icmp_payload, &request.header, 8);
    memcpy(icmp_payload + 8, request.payload, request.payload_size);
    
    // Envoyer la requête
    printf("%s[→]%s Envoi de la requête handshake au serveur...\n", 
           COLOR_YELLOW, COLOR_RESET);
    
    uint8_t reply_payload[ICMP_MTU];
    size_t reply_size;
    result = icmp_send_reliable(icmp_session, icmp_payload, 8 + request.payload_size,
                                 reply_payload, &reply_size);
    
    if (result != ICMP_SUCCESS) {
        printf("%s[✗]%s Échec envoi handshake: %s\n", 
               COLOR_RED, COLOR_RESET, icmp_error_string(result));
        return -1;
    }
    
    printf("%s[←]%s Réponse handshake reçue (%zu bytes)\n", 
           COLOR_YELLOW, COLOR_RESET, reply_size);
    
    // Parser la réponse
    proto_packet_t response;
    if (reply_size < 8) {
        printf("%s[✗]%s Réponse trop courte\n", COLOR_RED, COLOR_RESET);
        return -1;
    }
    
    memcpy(&response.header, reply_payload, 8);
    response.payload_size = reply_size - 8;
    memcpy(response.payload, reply_payload + 8, response.payload_size);
    
    // Finaliser le handshake côté client
    result = handshake_client_finish(hs_ctx, &response);
    if (result != HANDSHAKE_SUCCESS) {
        printf("%s[✗]%s Échec finalisation handshake: %s\n", 
               COLOR_RED, COLOR_RESET, handshake_error_string(result));
        return -1;
    }
    
    // Récupérer la clé de session
    uint8_t session_key[32];
    handshake_get_session_key(hs_ctx, session_key);
    
    printf("%s[✓]%s Handshake réussi ! Clé de session dérivée (AES-256)\n", 
           COLOR_GREEN, COLOR_RESET);
    print_hex("    Clé de session", session_key, 32, 16);
    
    return 0;
}

/**
 * Envoyer une requête HTTP encapsulée
 */
int send_http_request(icmp_session_t *icmp_session, 
                      const uint8_t session_key[32],
                      const char *host, const char *path) {
    printf("\n%s[*]%s Préparation de la requête HTTP...\n", 
           COLOR_CYAN, COLOR_RESET);
    
    // Créer le paquet IP/TCP/HTTP
    uint8_t ip_packet[2048];
    size_t ip_size = create_http_get_packet(host, path, 
                                             "10.0.1.2",  // Client virtuel
                                             "10.0.1.1",  // Serveur TUN
                                             ip_packet, sizeof(ip_packet));
    
    if (ip_size == 0) {
        printf("%s[✗]%s Échec création paquet HTTP\n", COLOR_RED, COLOR_RESET);
        return -1;
    }
    
    // Encapsuler dans le protocole ICMP tunnel
    printf("\n%s[*]%s Encapsulation et chiffrement...\n", COLOR_CYAN, COLOR_RESET);
    
    proto_packet_t packets[10];  // Support pour fragmentation
    size_t packet_count;
    
    // IMPORTANT: Use data sequence counter starting at 0 (separate from ICMP sequence)
    // This matches the server's data_sequence_counter initialization
    static uint16_t data_sequence = 0;
    
    int result = proto_encapsulate(ip_packet, ip_size,
                                    session_key,
                                    icmp_session->session_id,
                                    data_sequence,  // Use data sequence, not ICMP sequence
                                    packets, &packet_count, 10);
    
    if (result != PROTO_SUCCESS) {
        printf("%s[✗]%s Échec encapsulation: %s\n", 
               COLOR_RED, COLOR_RESET, proto_error_string(result));
        return -1;
    }
    
    printf("%s[+]%s Paquet encapsulé en %zu fragment(s)\n", 
           COLOR_GREEN, COLOR_RESET, packet_count);
    
    // Envoyer chaque fragment
    for (size_t i = 0; i < packet_count; i++) {
        proto_packet_t *pkt = &packets[i];
        
        printf("%s[→]%s Envoi fragment %zu/%zu (%zu bytes)...\n", 
               COLOR_YELLOW, COLOR_RESET, i + 1, packet_count, pkt->payload_size);
        
        // Construire le paquet ICMP complet
        uint8_t icmp_payload[ICMP_MTU];
        memcpy(icmp_payload, &pkt->header, 8);
        memcpy(icmp_payload + 8, pkt->payload, pkt->payload_size);
        size_t total_size = 8 + pkt->payload_size;
        
        // Envoyer
        result = icmp_send_packet(icmp_session, icmp_payload, total_size);
        if (result != ICMP_SUCCESS) {
            printf("%s[✗]%s Échec envoi fragment %zu: %s\n", 
                   COLOR_RED, COLOR_RESET, i + 1, icmp_error_string(result));
            return -1;
        }
        
        printf("%s[✓]%s Fragment %zu envoyé\n", COLOR_GREEN, COLOR_RESET, i + 1);
    }
    
    // Increment data sequence after successful send
    data_sequence++;
    
    return 0;
}

/**
 * Recevoir et décoder la réponse
 */
int receive_http_response(icmp_session_t *icmp_session, 
                          const uint8_t session_key[32]) {
    printf("\n%s[*]%s Attente de la réponse du serveur...\n", 
           COLOR_CYAN, COLOR_RESET);
    
    uint8_t reply_payload[ICMP_MTU];
    size_t reply_size;
    
    // Attendre la réponse (timeout 10 secondes)
    int result = icmp_wait_reply(icmp_session, reply_payload, &reply_size, 10000);
    
    if (result == ICMP_ERROR_TIMEOUT) {
        printf("%s[✗]%s Timeout - Aucune réponse reçue\n", COLOR_RED, COLOR_RESET);
        printf("    Le serveur est-il démarré ?\n");
        printf("    Le tunnel ICMP est-il configuré ?\n");
        return -1;
    } else if (result != ICMP_SUCCESS) {
        printf("%s[✗]%s Erreur réception: %s\n", 
               COLOR_RED, COLOR_RESET, icmp_error_string(result));
        return -1;
    }
    
    printf("%s[←]%s Réponse reçue (%zu bytes)\n", 
           COLOR_YELLOW, COLOR_RESET, reply_size);
    
    // Parser le paquet protocole
    if (reply_size < 8) {
        printf("%s[✗]%s Réponse trop courte\n", COLOR_RED, COLOR_RESET);
        return -1;
    }
    
    proto_packet_t response;
    memcpy(&response.header, reply_payload, 8);
    response.payload_size = reply_size - 8;
    memcpy(response.payload, reply_payload + 8, response.payload_size);
    
    printf("    Type: %s, Flags: 0x%02x\n", 
           proto_type_string(response.header.type), response.header.flags);
    
    // Décapsuler
    printf("\n%s[*]%s Déchiffrement et décapsulation...\n", 
           COLOR_CYAN, COLOR_RESET);
    
    uint8_t decrypted[2048];
    size_t decrypted_size;
    
    result = proto_decapsulate(&response, session_key,
                                icmp_session->session_id,
                                response.header.fragment_num,
                                decrypted, &decrypted_size, sizeof(decrypted));
    
    if (result != PROTO_SUCCESS) {
        printf("%s[✗]%s Échec décapsulation: %s\n", 
               COLOR_RED, COLOR_RESET, proto_error_string(result));
        return -1;
    }
    
    printf("%s[✓]%s Déchiffrement réussi (%zu bytes)\n", 
           COLOR_GREEN, COLOR_RESET, decrypted_size);
    
    // Afficher les données déchiffrées (IP packet)
    print_hex("    Paquet IP déchiffré", decrypted, decrypted_size, 64);
    
    // Tenter d'extraire le contenu HTTP (si présent)
    if (decrypted_size > 40) {  // IP(20) + TCP(20)
        uint8_t *tcp_data = decrypted + 40;
        size_t tcp_data_len = decrypted_size - 40;
        
        printf("\n%s[*]%s Données TCP (%zu bytes):\n%s", 
               COLOR_CYAN, COLOR_RESET, tcp_data_len, COLOR_RESET);
        
        // Afficher comme texte si c'est du HTTP
        for (size_t i = 0; i < tcp_data_len && i < 500; i++) {
            if (tcp_data[i] >= 32 && tcp_data[i] <= 126) {
                putchar(tcp_data[i]);
            } else if (tcp_data[i] == '\r' || tcp_data[i] == '\n') {
                putchar(tcp_data[i]);
            } else {
                putchar('.');
            }
        }
        printf("\n");
    }
    
    return 0;
}

/**
 * Programme principal
 */
int main(int argc, char *argv[]) {
    // Initialisation Winsock
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    // Parser les arguments
    if (argc < 2) {
        printf("Usage: %s <server_ip> [url]\n", argv[0]);
        printf("  server_ip : Adresse IP du serveur ICMP (ex: 192.168.1.100)\n");
        printf("  url       : URL à requêter (défaut: http://example.com)\n");
        printf("\nExample: %s 192.168.1.100 http://example.com\n", argv[0]);
        return 1;
    }
    
    const char *server_ip = argv[1];
    const char *url = (argc >= 3) ? argv[2] : "http://example.com";
    
    // Parser l'URL (simplifié)
    char host[256] = "example.com";
    char path[256] = "/";
    
    if (strstr(url, "http://") == url) {
        const char *host_start = url + 7;
        const char *path_start = strchr(host_start, '/');
        if (path_start) {
            size_t host_len = path_start - host_start;
            strncpy(host, host_start, host_len);
            host[host_len] = '\0';
            strncpy(path, path_start, sizeof(path) - 1);
        } else {
            strncpy(host, host_start, sizeof(host) - 1);
        }
    }
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║          TEST D'ENCAPSULATION ICMP - REQUÊTE RÉELLE            ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Serveur ICMP : %s\n", server_ip);
    printf("URL cible    : %s\n", url);
    printf("Host         : %s\n", host);
    printf("Path         : %s\n", path);
    printf("\n");
    
    srand((unsigned int)time(NULL));
    
    // Étape 1: Créer la session ICMP
    printf("%s[*]%s Création de la session ICMP...\n", COLOR_CYAN, COLOR_RESET);
    icmp_session_t *session = icmp_session_create(server_ip);
    if (!session) {
        printf("%s[✗]%s Échec création session ICMP\n", COLOR_RED, COLOR_RESET);
        return 1;
    }
    printf("%s[✓]%s Session ICMP créée (ID: 0x%04x)\n", 
           COLOR_GREEN, COLOR_RESET, session->session_id);
    
    // Skip connectivity test - go directly to handshake
    // (The server only responds to protocol packets, not raw pings)
    printf("\n%s[*]%s Skipping connectivity test (server requires protocol packets)\n",
           COLOR_CYAN, COLOR_RESET);
    
    // Étape 3: Handshake DH
    handshake_context_t hs_ctx;
    if (perform_handshake(session, &hs_ctx) != 0) {
        icmp_session_destroy(session);
        return 1;
    }
    
    // Récupérer la clé de session
    uint8_t session_key[32];
    handshake_get_session_key(&hs_ctx, session_key);
    
    // Étape 4: Envoyer la requête HTTP
    if (send_http_request(session, session_key, host, path) != 0) {
        handshake_cleanup(&hs_ctx);
        icmp_session_destroy(session);
        return 1;
    }
    
    // Étape 5: Recevoir la réponse
    if (receive_http_response(session, session_key) != 0) {
        handshake_cleanup(&hs_ctx);
        icmp_session_destroy(session);
        return 1;
    }
    
    // Afficher les statistiques
    printf("\n%s[*]%s Statistiques de session:\n", COLOR_CYAN, COLOR_RESET);
    uint32_t sent, recv, lost, avg_rtt;
    icmp_get_statistics(session, &sent, &recv, &lost, &avg_rtt);
    printf("    Paquets envoyés    : %u\n", sent);
    printf("    Paquets reçus      : %u\n", recv);
    printf("    Paquets perdus     : %u\n", lost);
    printf("    RTT moyen          : %u ms\n", avg_rtt);
    
    // Nettoyage
    printf("\n%s[*]%s Nettoyage...\n", COLOR_CYAN, COLOR_RESET);
    handshake_cleanup(&hs_ctx);
    icmp_session_destroy(session);
    
    printf("\n%s[✓]%s Test terminé avec succès !\n\n", COLOR_GREEN, COLOR_RESET);
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    return 0;
}
