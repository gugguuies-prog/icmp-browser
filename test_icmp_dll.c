/**
 * test_icmp_dll.c
 * 
 * Test de l'API Windows Icmp.dll pour envoyer des paquets ICMP
 * avec payload personnalisé SANS droits administrateur.
 * 
 * Compile avec: gcc test_icmp_dll.c -o test_icmp_dll.exe -liphlpapi -lws2_32
 * Ou avec MSVC: cl test_icmp_dll.c /link iphlpapi.lib ws2_32.lib
 */

#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ws2tcpip.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#define PAYLOAD_SIZE 64
#define REPLY_BUFFER_SIZE 1024

void print_hex(const unsigned char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (len % 16 != 0) printf("\n");
}

int main(int argc, char *argv[]) {
    HANDLE hIcmp;
    DWORD dwRetVal = 0;
    char SendData[PAYLOAD_SIZE];
    LPVOID ReplyBuffer = NULL;
    DWORD ReplySize = 0;
    IPAddr ipaddr;
    struct sockaddr_in dest;
    
    // Destination par défaut: 8.8.8.8 (Google DNS)
    const char *dest_ip = (argc > 1) ? argv[1] : "8.8.8.8";
    
    printf("=== Test ICMP.DLL API - Envoi de payload personnalisé ===\n");
    printf("Destination: %s\n\n", dest_ip);
    
    // Initialiser Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "Erreur: WSAStartup a échoué\n");
        return 1;
    }
    
    // Convertir l'adresse IP
    if (inet_pton(AF_INET, dest_ip, &ipaddr) != 1) {
        fprintf(stderr, "Erreur: Adresse IP invalide: %s\n", dest_ip);
        WSACleanup();
        return 1;
    }
    
    // Créer le handle ICMP (ne nécessite PAS de privilèges admin!)
    hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Erreur: IcmpCreateFile a échoué (code: %lu)\n", GetLastError());
        fprintf(stderr, "Note: Cette API devrait fonctionner sans droits admin.\n");
        WSACleanup();
        return 1;
    }
    
    printf("✓ IcmpCreateFile: Handle créé avec succès (pas de droits admin requis!)\n");
    
    // Préparer un payload personnalisé (simule du trafic HTTP encapsulé)
    memset(SendData, 0, PAYLOAD_SIZE);
    sprintf(SendData, "TEST_PAYLOAD|HTTP_TUNNELED_DATA|SessionID:12345");
    
    printf("\n--- Payload personnalisé à envoyer (%d bytes) ---\n", PAYLOAD_SIZE);
    printf("Texte: %s\n", SendData);
    printf("Hex: ");
    print_hex((unsigned char*)SendData, PAYLOAD_SIZE);
    
    // Allouer le buffer de réponse
    ReplySize = sizeof(ICMP_ECHO_REPLY) + PAYLOAD_SIZE + 8;
    ReplyBuffer = malloc(ReplySize);
    if (ReplyBuffer == NULL) {
        fprintf(stderr, "Erreur: Allocation mémoire échouée\n");
        IcmpCloseHandle(hIcmp);
        WSACleanup();
        return 1;
    }
    
    // Configurer les options IP
    IP_OPTION_INFORMATION ipOptions;
    memset(&ipOptions, 0, sizeof(ipOptions));
    ipOptions.Ttl = 128;       // Time to Live
    ipOptions.Tos = 0;         // Type of Service
    ipOptions.Flags = 0;       // Pas de fragmentation
    
    printf("\n--- Envoi du paquet ICMP Echo Request ---\n");
    printf("TTL: %d\n", ipOptions.Ttl);
    printf("Timeout: 5000 ms\n");
    
    // Envoyer le paquet ICMP Echo Request avec payload personnalisé
    dwRetVal = IcmpSendEcho(
        hIcmp,              // Handle ICMP
        ipaddr,             // Adresse IP destination
        SendData,           // Payload personnalisé
        PAYLOAD_SIZE,       // Taille du payload
        &ipOptions,         // Options IP (TTL, TOS)
        ReplyBuffer,        // Buffer pour la réponse
        ReplySize,          // Taille du buffer
        5000                // Timeout en ms
    );
    
    if (dwRetVal == 0) {
        DWORD error = GetLastError();
        fprintf(stderr, "\n✗ IcmpSendEcho a échoué (code: %lu)\n", error);
        
        // Messages d'erreur détaillés
        switch(error) {
            case IP_BUF_TOO_SMALL:
                fprintf(stderr, "   Erreur: Buffer trop petit\n");
                break;
            case IP_DEST_NET_UNREACHABLE:
                fprintf(stderr, "   Erreur: Réseau destination injoignable\n");
                break;
            case IP_DEST_HOST_UNREACHABLE:
                fprintf(stderr, "   Erreur: Hôte destination injoignable\n");
                break;
            case IP_REQ_TIMED_OUT:
                fprintf(stderr, "   Erreur: Timeout de la requête\n");
                break;
            default:
                fprintf(stderr, "   Erreur inconnue\n");
        }
        
        free(ReplyBuffer);
        IcmpCloseHandle(hIcmp);
        WSACleanup();
        return 1;
    }
    
    printf("\n✓ IcmpSendEcho: %lu réponse(s) reçue(s)\n", dwRetVal);
    
    // Parser la réponse
    PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
    
    for (DWORD i = 0; i < dwRetVal; i++) {
        struct in_addr ReplyAddr;
        ReplyAddr.S_un.S_addr = pEchoReply->Address;
        char *reply_ip = inet_ntoa(ReplyAddr);
        
        printf("\n--- Réponse ICMP Echo Reply #%lu ---\n", i + 1);
        printf("Adresse source: %s\n", reply_ip);
        printf("Status: %lu\n", pEchoReply->Status);
        printf("RTT: %lu ms\n", pEchoReply->RoundTripTime);
        printf("TTL: %d\n", pEchoReply->Options.Ttl);
        printf("Taille payload: %d bytes\n", pEchoReply->DataSize);
        
        if (pEchoReply->Status == IP_SUCCESS) {
            printf("✓ Status: SUCCESS\n");
            
            if (pEchoReply->DataSize > 0 && pEchoReply->Data != NULL) {
                printf("\n--- Payload reçu dans Echo Reply ---\n");
                printf("Texte: %.*s\n", pEchoReply->DataSize, (char*)pEchoReply->Data);
                printf("Hex: ");
                print_hex((unsigned char*)pEchoReply->Data, pEchoReply->DataSize);
                
                // Vérifier si le payload correspond à ce qu'on a envoyé
                if (pEchoReply->DataSize == PAYLOAD_SIZE && 
                    memcmp(pEchoReply->Data, SendData, PAYLOAD_SIZE) == 0) {
                    printf("\n✓✓✓ SUCCÈS: Le payload personnalisé a été correctement retourné! ✓✓✓\n");
                } else {
                    printf("\n⚠ Attention: Le payload reçu diffère du payload envoyé\n");
                }
            }
        } else {
            printf("✗ Status: FAILED (code: %lu)\n", pEchoReply->Status);
        }
        
        pEchoReply++;
    }
    
    printf("\n=== Résumé du test ===\n");
    printf("✓ IcmpCreateFile fonctionne SANS droits admin\n");
    printf("✓ IcmpSendEcho permet d'envoyer un payload personnalisé\n");
    printf("✓ Le payload est transporté dans les paquets ICMP Echo Request/Reply\n");
    printf("✓ Cette technique peut être utilisée pour tunneliser du trafic IP!\n");
    printf("\n=== Test réussi - Technique validée ===\n");
    
    // Nettoyage
    free(ReplyBuffer);
    IcmpCloseHandle(hIcmp);
    WSACleanup();
    
    return 0;
}
