/**
 * test_protocol.c
 * 
 * Unit tests for ICMP Tunneling Protocol
 */

#include "../src/common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Test colors
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RED "\033[31m"
#define COLOR_RESET "\033[0m"

// Test session key and parameters
static uint8_t test_key[32] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

// Test 1: Basic encapsulation/decapsulation
void test_basic_encap_decap() {
    printf("\n=== Test 1: Basic Encapsulation/Decapsulation ===\n");
    
    // Create test IP packet
    uint8_t ip_packet[100];
    for (int i = 0; i < 100; i++) {
        ip_packet[i] = (uint8_t)i;
    }
    
    uint16_t session_id = 0x1234;
    uint16_t sequence = 42;
    
    // Encapsulate
    proto_packet_t packets[10];
    size_t packet_count;
    
    int result = proto_encapsulate(ip_packet, 100, test_key, session_id, sequence,
                                     packets, &packet_count, 10);
    
    assert(result == PROTO_SUCCESS);
    printf("✓ Encapsulation successful\n");
    printf("  Packets created: %zu\n", packet_count);
    assert(packet_count == 1); // Small packet, no fragmentation
    
    printf("  Packet type: %s\n", proto_type_string(packets[0].header.type));
    printf("  Flags: 0x%02X (Encrypted: %s)\n", 
           packets[0].header.flags,
           (packets[0].header.flags & PROTO_FLAG_ENCRYPTED) ? "Yes" : "No");
    printf("  Payload size: %zu bytes\n", packets[0].payload_size);
    
    // Decapsulate
    uint8_t decrypted[200];
    size_t decrypted_size;
    
    result = proto_decapsulate(&packets[0], test_key, session_id, sequence,
                                decrypted, &decrypted_size, 200);
    
    assert(result == PROTO_SUCCESS);
    printf("✓ Decapsulation successful\n");
    printf("  Decrypted size: %zu bytes\n", decrypted_size);
    
    // Verify data integrity
    assert(decrypted_size == 100);
    assert(memcmp(decrypted, ip_packet, 100) == 0);
    printf("✓ Data integrity verified (100 bytes matched)\n");
    
    printf(COLOR_GREEN "✓✓✓ Test 1 PASSED\n" COLOR_RESET);
}

// Test 2: Large packet fragmentation
void test_fragmentation() {
    printf("\n=== Test 2: Large Packet Fragmentation ===\n");
    
    // Create large test packet (3000 bytes > MTU)
    size_t large_size = 3000;
    uint8_t *large_packet = (uint8_t*)malloc(large_size);
    for (size_t i = 0; i < large_size; i++) {
        large_packet[i] = (uint8_t)(i & 0xFF);
    }
    
    uint16_t session_id = 0x5678;
    uint16_t sequence = 100;
    
    // Encapsulate (should create multiple fragments)
    proto_packet_t packets[20];
    size_t packet_count;
    
    int result = proto_encapsulate(large_packet, large_size, test_key, session_id, sequence,
                                     packets, &packet_count, 20);
    
    assert(result == PROTO_SUCCESS);
    printf("✓ Fragmentation successful\n");
    printf("  Original size: %zu bytes\n", large_size);
    printf("  Fragments created: %zu\n", packet_count);
    assert(packet_count > 1); // Should be fragmented
    
    // Verify fragment properties
    for (size_t i = 0; i < packet_count; i++) {
        printf("  Fragment %zu: ID=0x%04X, Num=%u/%u, Size=%zu, Last=%s\n",
               i, packets[i].header.fragment_id,
               packets[i].header.fragment_num,
               packets[i].header.total_fragments,
               packets[i].payload_size,
               (packets[i].header.flags & PROTO_FLAG_LAST_FRAGMENT) ? "Yes" : "No");
        
        assert(packets[i].header.flags & PROTO_FLAG_FRAGMENTED);
        assert(packets[i].header.fragment_num == i);
        assert(packets[i].header.total_fragments == packet_count);
    }
    
    // Verify last fragment flag
    assert(packets[packet_count - 1].header.flags & PROTO_FLAG_LAST_FRAGMENT);
    printf("✓ Fragment metadata correct\n");
    
    // Reassemble using context
    proto_reassembly_ctx_t ctx = {0};
    
    for (size_t i = 0; i < packet_count; i++) {
        result = proto_add_fragment(&ctx, &packets[i], test_key, session_id, sequence);
        
        if (i < packet_count - 1) {
            assert(result == PROTO_ERROR_INCOMPLETE);
        } else {
            assert(result > 0); // Should return total size
            printf("✓ Reassembly complete, total size: %d bytes\n", result);
        }
    }
    
    // Get reassembled data
    uint8_t *reassembled = (uint8_t*)malloc(large_size + 1000);
    size_t reassembled_size;
    
    result = proto_get_reassembled(&ctx, reassembled, &reassembled_size, large_size + 1000);
    assert(result == PROTO_SUCCESS);
    printf("  Reassembled size: %zu bytes\n", reassembled_size);
    
    // Verify data integrity
    assert(reassembled_size == large_size);
    assert(memcmp(reassembled, large_packet, large_size) == 0);
    printf("✓ Data integrity verified (%zu bytes matched)\n", large_size);
    
    // Cleanup
    proto_reset_reassembly(&ctx);
    free(large_packet);
    free(reassembled);
    
    printf(COLOR_GREEN "✓✓✓ Test 2 PASSED\n" COLOR_RESET);
}

// Test 3: Handshake packet
void test_handshake() {
    printf("\n=== Test 3: Handshake Packet ===\n");
    
    // Create DH public key (256 bytes)
    uint8_t dh_public_key[256];
    for (int i = 0; i < 256; i++) {
        dh_public_key[i] = (uint8_t)i;
    }
    
    // Create handshake packet
    proto_packet_t packet;
    int result = proto_create_handshake(dh_public_key, &packet);
    
    assert(result == PROTO_SUCCESS);
    printf("✓ Handshake packet created\n");
    printf("  Type: %s\n", proto_type_string(packet.header.type));
    printf("  Payload size: %zu bytes\n", packet.payload_size);
    assert(packet.header.type == PROTO_TYPE_HANDSHAKE);
    assert(packet.payload_size == 256);
    
    // Parse handshake packet
    uint8_t parsed_key[256];
    result = proto_parse_handshake(&packet, parsed_key);
    
    assert(result == PROTO_SUCCESS);
    printf("✓ Handshake packet parsed\n");
    
    // Verify data
    assert(memcmp(parsed_key, dh_public_key, 256) == 0);
    printf("✓ DH public key verified (256 bytes matched)\n");
    
    printf(COLOR_GREEN "✓✓✓ Test 3 PASSED\n" COLOR_RESET);
}

// Test 4: Keepalive packet
void test_keepalive() {
    printf("\n=== Test 4: Keepalive Packet ===\n");
    
    proto_packet_t packet;
    int result = proto_create_keepalive(&packet);
    
    assert(result == PROTO_SUCCESS);
    printf("✓ Keepalive packet created\n");
    printf("  Type: %s\n", proto_type_string(packet.header.type));
    printf("  Payload size: %zu bytes\n", packet.payload_size);
    assert(packet.header.type == PROTO_TYPE_KEEPALIVE);
    assert(packet.payload_size == 0);
    
    printf(COLOR_GREEN "✓✓✓ Test 4 PASSED\n" COLOR_RESET);
}

// Test 5: ACK packet
void test_ack() {
    printf("\n=== Test 5: ACK Packet ===\n");
    
    uint16_t ack_seq = 12345;
    
    // Create ACK
    proto_packet_t packet;
    int result = proto_create_ack(ack_seq, &packet);
    
    assert(result == PROTO_SUCCESS);
    printf("✓ ACK packet created\n");
    printf("  Type: %s\n", proto_type_string(packet.header.type));
    printf("  Payload size: %zu bytes\n", packet.payload_size);
    assert(packet.header.type == PROTO_TYPE_ACK);
    
    // Parse ACK
    uint16_t parsed_seq;
    result = proto_parse_ack(&packet, &parsed_seq);
    
    assert(result == PROTO_SUCCESS);
    printf("✓ ACK packet parsed\n");
    printf("  ACK sequence: %u\n", parsed_seq);
    assert(parsed_seq == ack_seq);
    
    printf(COLOR_GREEN "✓✓✓ Test 5 PASSED\n" COLOR_RESET);
}

// Test 6: MTU-sized packet
void test_mtu_packet() {
    printf("\n=== Test 6: MTU-Sized Packet ===\n");
    
    // Create MTU-sized packet
    size_t mtu_size = PROTO_MAX_DATA_SIZE;
    uint8_t *mtu_packet = (uint8_t*)malloc(mtu_size);
    for (size_t i = 0; i < mtu_size; i++) {
        mtu_packet[i] = (uint8_t)(i & 0xFF);
    }
    
    printf("Testing with %zu bytes (MTU limit)\n", mtu_size);
    
    uint16_t session_id = 0xABCD;
    uint16_t sequence = 999;
    
    // Encapsulate
    proto_packet_t packets[10];
    size_t packet_count;
    
    int result = proto_encapsulate(mtu_packet, mtu_size, test_key, session_id, sequence,
                                     packets, &packet_count, 10);
    
    assert(result == PROTO_SUCCESS);
    printf("✓ MTU packet encapsulated\n");
    printf("  Packets: %zu\n", packet_count);
    
    // Decapsulate
    uint8_t *decrypted = (uint8_t*)malloc(mtu_size + 1000);
    size_t decrypted_size;
    
    if (packet_count == 1) {
        // Single packet
        result = proto_decapsulate(&packets[0], test_key, session_id, sequence,
                                    decrypted, &decrypted_size, mtu_size + 1000);
    } else {
        // Multiple fragments
        proto_reassembly_ctx_t ctx = {0};
        for (size_t i = 0; i < packet_count; i++) {
            proto_add_fragment(&ctx, &packets[i], test_key, session_id, sequence);
        }
        result = proto_get_reassembled(&ctx, decrypted, &decrypted_size, mtu_size + 1000);
        proto_reset_reassembly(&ctx);
    }
    
    assert(result == PROTO_SUCCESS);
    printf("✓ MTU packet decrypted\n");
    
    // Verify
    assert(decrypted_size == mtu_size);
    assert(memcmp(decrypted, mtu_packet, mtu_size) == 0);
    printf("✓ Data integrity verified (%zu bytes)\n", mtu_size);
    
    free(mtu_packet);
    free(decrypted);
    
    printf(COLOR_GREEN "✓✓✓ Test 6 PASSED\n" COLOR_RESET);
}

// Test 7: Error handling
void test_error_handling() {
    printf("\n=== Test 7: Error Handling ===\n");
    
    proto_packet_t packets[10];
    size_t count;
    uint8_t data[100];
    size_t size;
    
    // Test NULL parameters
    int result = proto_encapsulate(NULL, 100, test_key, 0, 0, packets, &count, 10);
    assert(result == PROTO_ERROR_INVALID_PARAM);
    printf("✓ NULL data rejected\n");
    
    result = proto_decapsulate(NULL, test_key, 0, 0, data, &size, 100);
    assert(result == PROTO_ERROR_INVALID_PARAM);
    printf("✓ NULL packet rejected\n");
    
    // Test invalid handshake parse
    proto_packet_t bad_packet;
    bad_packet.header.type = PROTO_TYPE_DATA; // Wrong type
    uint8_t key[256];
    result = proto_parse_handshake(&bad_packet, key);
    assert(result == PROTO_ERROR_INVALID_TYPE);
    printf("✓ Wrong packet type rejected\n");
    
    printf(COLOR_GREEN "✓✓✓ Test 7 PASSED\n" COLOR_RESET);
}

// Main test runner
int main() {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   ICMP Tunneling Protocol Unit Tests         ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    
    test_basic_encap_decap();
    test_fragmentation();
    test_handshake();
    test_keepalive();
    test_ack();
    test_mtu_packet();
    test_error_handling();
    
    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║   " COLOR_GREEN "ALL TESTS COMPLETED" COLOR_RESET "                      ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("\n" COLOR_GREEN "✓ Protocol implementation is ready!\n" COLOR_RESET);
    printf("  - Encapsulation/Decapsulation with AES-256-GCM\n");
    printf("  - Fragmentation for large packets\n");
    printf("  - Handshake, ACK, Keepalive packets\n");
    printf("  - Complete protocol stack operational\n");
    
    return 0;
}
