# Makefile for ICMP Browser Project
# Supports both Linux (native) and Windows (MinGW cross-compile via WSL)

# Detect platform
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    # Check if we're in WSL
    IS_WSL := $(shell grep -qi microsoft /proc/version && echo 1 || echo 0)
    ifeq ($(IS_WSL),1)
        PLATFORM := WSL
    else
        PLATFORM := LINUX
    endif
else
    PLATFORM := WINDOWS
endif

# Compiler settings
ifeq ($(PLATFORM),WSL)
    # Cross-compile for Windows from WSL
    CC_WIN := x86_64-w64-mingw32-gcc
    CC_LINUX := gcc
else ifeq ($(PLATFORM),LINUX)
    CC_LINUX := gcc
else
    # Native Windows (MinGW)
    CC_WIN := gcc
endif

# Common flags
CFLAGS := -Wall -Wextra -O2 -Isrc
LDFLAGS_CRYPTO := -lcrypto -lssl

# Windows-specific flags
LDFLAGS_WIN := -liphlpapi -lws2_32 $(LDFLAGS_CRYPTO)

# Linux-specific flags
LDFLAGS_LINUX := -lpthread $(LDFLAGS_CRYPTO) -lz

# Source files
SRC_CRYPTO := src/crypto/aes_gcm.c src/crypto/diffie_hellman.c
SRC_CLIENT := src/client/icmp_engine.c src/client/http_proxy.c
SRC_SERVER := src/server/icmp_handler.c src/server/session_manager.c src/server/tun_device.c
SRC_COMMON := src/common/protocol.c src/common/handshake.c src/common/priority_queue.c src/common/compression.c src/common/statistics.c

# Main programs
SERVER_MAIN := src/server/main.c

# Production server (includes all modules)
SERVER_FULL := $(SERVER_MAIN) $(SRC_SERVER) $(SRC_COMMON) $(SRC_CRYPTO)

# Test files
TEST_AES_GCM := tests/test_aes_gcm.c
TEST_DH := tests/test_dh.c
TEST_ICMP_ENGINE := tests/test_icmp_engine.c
TEST_ICMP_HANDLER := tests/test_icmp_handler.c
TEST_PROTOCOL := tests/test_protocol.c
TEST_HANDSHAKE := tests/test_handshake.c
TEST_SESSION_MANAGER := tests/test_session_manager.c
TEST_TUN_DEVICE := tests/test_tun_device.c
TEST_PRIORITY_QUEUE := tests/test_priority_queue.c
TEST_COMPRESSION := tests/test_compression.c
TEST_STATISTICS := tests/test_statistics.c
TEST_HTTP_PROXY := tests/test_http_proxy.c

# Output directories
BUILD_DIR := build
BIN_DIR := bin

# Targets
.PHONY: all clean test test-crypto help dirs server

all: dirs test-crypto-win

dirs:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(BUILD_DIR)

# ============================================================================
# Production Server (Linux only)
# ============================================================================
server: dirs
	@echo "========================================="
	@echo " Building Production ICMP Server"
	@echo "========================================="
	@echo ""
	@echo "📦 Compiling icmp_server..."
	$(CC_LINUX) $(CFLAGS) $(SERVER_FULL) -o $(BIN_DIR)/icmp_server $(LDFLAGS_LINUX)
	@echo ""
	@echo "✅ Server built: $(BIN_DIR)/icmp_server"
	@echo ""
	@echo "🚀 To run:"
	@echo "   sudo ./$(BIN_DIR)/icmp_server"
	@echo ""
	@echo "⚙️  Options:"
	@echo "   -i <interface>  Network interface (default: eth0)"
	@echo "   -v              Verbose logging"
	@echo "   -h              Show help"
	@echo ""

	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Help target
help:
	@echo "ICMP Browser Makefile"
	@echo "Platform detected: $(PLATFORM)"
	@echo ""
	@echo "Available targets:"
	@echo "  make server                - Build production ICMP server (Linux)"
	@echo "  make test-crypto-win       - Build and run AES-GCM tests (Windows)"
	@echo "  make test-crypto-linux     - Build and run AES-GCM tests (Linux)"
	@echo "  make test-dh-win           - Build and run DH tests (Windows)"
	@echo "  make test-dh-linux         - Build and run DH tests (Linux)"
	@echo "  make test-icmp-engine-win  - Build and run ICMP Engine tests (Windows)"
	@echo "  make test-icmp-handler-linux - Build and run ICMP Handler tests (Linux)"
	@echo "  make test-protocol-linux   - Build and run Protocol tests (Linux)"
	@echo "  make test-handshake-linux  - Build and run Handshake tests (Linux)"
	@echo "  make test-session-manager-linux - Build and run Session Manager tests (Linux)"
	@echo "  make test-tun-device-linux - Build and run TUN Device tests (Linux, requires root)"
	@echo "  make test-priority-queue-linux - Build and run Priority Queue tests (Linux)"
	@echo "  make test-compression-linux - Build and run Compression tests (Linux)"
	@echo "  make test-statistics-linux - Build and run Statistics tests (Linux)"
	@echo "  make test-http-proxy-win   - Build and run HTTP Proxy tests (Windows)"
	@echo "  make test-http-proxy-linux - Build and run HTTP Proxy tests (Linux)"
	@echo "  make test-all-linux        - Run all crypto tests (Linux)"
	@echo "  make test-icmp-dll         - Build test_icmp_dll.exe (Windows)"
	@echo "  make clean                 - Clean build artifacts"
	@echo "  make help                  - Show this help"

# AES-GCM tests for Windows
test-crypto-win: dirs
	@echo "Building AES-GCM tests for Windows..."
	$(CC_WIN) $(CFLAGS) $(TEST_AES_GCM) $(SRC_CRYPTO) -o $(BIN_DIR)/test_aes_gcm.exe $(LDFLAGS_WIN)
	@echo "✓ Built: $(BIN_DIR)/test_aes_gcm.exe"
	@echo ""
	@echo "Running tests..."
	@./$(BIN_DIR)/test_aes_gcm.exe

# AES-GCM tests for Linux
test-crypto-linux: dirs
	@echo "Building AES-GCM tests for Linux..."
	$(CC_LINUX) $(CFLAGS) $(TEST_AES_GCM) $(SRC_CRYPTO) -o $(BIN_DIR)/test_aes_gcm $(LDFLAGS_LINUX)
	@echo "✓ Built: $(BIN_DIR)/test_aes_gcm"
	@echo ""
	@echo "Running tests..."
	@./$(BIN_DIR)/test_aes_gcm

# DH tests for Windows
test-dh-win: dirs
	@echo "Building DH tests for Windows..."
	$(CC_WIN) $(CFLAGS) $(TEST_DH) $(SRC_CRYPTO) -o $(BIN_DIR)/test_dh.exe $(LDFLAGS_WIN)
	@echo "✓ Built: $(BIN_DIR)/test_dh.exe"
	@echo ""
	@echo "Running tests..."
	@./$(BIN_DIR)/test_dh.exe

# DH tests for Linux
test-dh-linux: dirs
	@echo "Building DH tests for Linux..."
	$(CC_LINUX) $(CFLAGS) $(TEST_DH) $(SRC_CRYPTO) -o $(BIN_DIR)/test_dh $(LDFLAGS_LINUX)
	@echo "✓ Built: $(BIN_DIR)/test_dh"
	@echo ""
	@echo "Running tests..."
	@./$(BIN_DIR)/test_dh

# All crypto tests for Linux
test-all-linux: test-crypto-linux test-dh-linux
	@echo ""
	@echo "═══════════════════════════════════════"
	@echo "  ALL CRYPTO TESTS COMPLETED"
	@echo "═══════════════════════════════════════"

# ICMP Engine tests for Windows
test-icmp-engine-win: dirs
	@echo "Building ICMP Engine tests for Windows..."
	$(CC_WIN) $(CFLAGS) $(TEST_ICMP_ENGINE) $(SRC_CLIENT) -o $(BIN_DIR)/test_icmp_engine.exe -liphlpapi -lws2_32
	@echo "✓ Built: $(BIN_DIR)/test_icmp_engine.exe"
	@echo ""
	@echo "Running tests..."
	@./$(BIN_DIR)/test_icmp_engine.exe

# ICMP Handler tests for Linux
test-icmp-handler-linux: dirs
	@echo "Building ICMP Handler tests for Linux..."
	$(CC_LINUX) $(CFLAGS) $(TEST_ICMP_HANDLER) $(SRC_SERVER) -o $(BIN_DIR)/test_icmp_handler $(LDFLAGS_LINUX)
	@echo "✓ Built: $(BIN_DIR)/test_icmp_handler"
	@echo ""
	@echo "Running tests (requires root)..."
	@echo "Note: Run with: sudo ./$(BIN_DIR)/test_icmp_handler"

# Protocol tests for Linux
test-protocol-linux: dirs
	@echo "Building Protocol tests for Linux..."
	$(CC_LINUX) $(CFLAGS) $(TEST_PROTOCOL) $(SRC_COMMON) $(SRC_CRYPTO) -o $(BIN_DIR)/test_protocol $(LDFLAGS_LINUX)
	@echo "✓ Built: $(BIN_DIR)/test_protocol"
	@echo ""
	@echo "Running tests..."
	@./$(BIN_DIR)/test_protocol

# Handshake tests for Linux
test-handshake-linux: dirs
	@echo "Building Handshake tests for Linux..."
	$(CC_LINUX) $(CFLAGS) $(TEST_HANDSHAKE) $(SRC_COMMON) $(SRC_CRYPTO) -o $(BIN_DIR)/test_handshake $(LDFLAGS_LINUX)
	@echo "✓ Built: $(BIN_DIR)/test_handshake"
	@echo ""
	@echo "Running tests..."
	@./$(BIN_DIR)/test_handshake

# Session Manager tests for Linux
test-session-manager-linux: dirs
	@echo "Building Session Manager tests for Linux..."
	$(CC_LINUX) $(CFLAGS) $(TEST_SESSION_MANAGER) src/server/session_manager.c -o $(BIN_DIR)/test_session_manager $(LDFLAGS_LINUX)
	@echo "✓ Built: $(BIN_DIR)/test_session_manager"
	@echo ""
	@echo "Running tests..."
	@./$(BIN_DIR)/test_session_manager

# TUN Device tests for Linux
test-tun-device-linux: dirs
	@echo "Building TUN Device tests for Linux..."
	$(CC_LINUX) $(CFLAGS) $(TEST_TUN_DEVICE) src/server/tun_device.c -o $(BIN_DIR)/test_tun_device $(LDFLAGS_LINUX)
	@echo "✓ Built: $(BIN_DIR)/test_tun_device"
	@echo ""
	@echo "Running tests (requires root)..."
	@echo "Note: Run with: sudo ./$(BIN_DIR)/test_tun_device"

# Priority Queue tests for Linux
test-priority-queue-linux: dirs
	@echo "Building Priority Queue tests for Linux..."
	$(CC_LINUX) $(CFLAGS) $(TEST_PRIORITY_QUEUE) src/common/priority_queue.c -o $(BIN_DIR)/test_priority_queue $(LDFLAGS_LINUX)
	@echo "✓ Built: $(BIN_DIR)/test_priority_queue"
	@echo ""
	@echo "Running tests..."
	@./$(BIN_DIR)/test_priority_queue

# Compression tests for Linux
test-compression-linux: dirs
	@echo "Building Compression tests for Linux..."
	$(CC_LINUX) $(CFLAGS) $(TEST_COMPRESSION) src/common/compression.c -o $(BIN_DIR)/test_compression $(LDFLAGS_LINUX)
	@echo "✓ Built: $(BIN_DIR)/test_compression"
	@echo ""
	@echo "Running tests..."
	@./$(BIN_DIR)/test_compression

# Statistics tests for Linux
test-statistics-linux: dirs
	@echo "Building Statistics tests for Linux..."
	$(CC_LINUX) $(CFLAGS) $(TEST_STATISTICS) src/common/statistics.c -o $(BIN_DIR)/test_statistics $(LDFLAGS_LINUX)
	@echo "✓ Built: $(BIN_DIR)/test_statistics"
	@echo ""
	@echo "Running tests..."
	@./$(BIN_DIR)/test_statistics

# HTTP Proxy tests for Windows
test-http-proxy-win: dirs
	@echo "Building HTTP Proxy tests for Windows..."
	$(CC_WIN) $(CFLAGS) $(TEST_HTTP_PROXY) src/client/http_proxy.c -o $(BIN_DIR)/test_http_proxy.exe -liphlpapi -lws2_32 -lpthread
	@echo "✓ Built: $(BIN_DIR)/test_http_proxy.exe"
	@echo ""
	@echo "Running tests..."
	@./$(BIN_DIR)/test_http_proxy.exe

# HTTP Proxy tests for Linux
test-http-proxy-linux: dirs
	@echo "Building HTTP Proxy tests for Linux..."
	$(CC_LINUX) $(CFLAGS) $(TEST_HTTP_PROXY) src/client/http_proxy.c -o $(BIN_DIR)/test_http_proxy $(LDFLAGS_LINUX)
	@echo "✓ Built: $(BIN_DIR)/test_http_proxy"
	@echo ""
	@echo "Running tests..."
	@./$(BIN_DIR)/test_http_proxy

test-icmp-dll: dirs
	@echo "Building test_icmp_dll.exe..."
	$(CC_WIN) $(CFLAGS) test_icmp_dll.c -o $(BIN_DIR)/test_icmp_dll.exe $(LDFLAGS_WIN)
	@echo "✓ Built: $(BIN_DIR)/test_icmp_dll.exe"
	@echo ""
	@echo "Running ICMP test..."
	@./$(BIN_DIR)/test_icmp_dll.exe 8.8.8.8

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@rm -f test_icmp_dll.exe test_aes_gcm.exe
	@echo "✓ Clean complete"

# Quick test target (platform-specific)
ifeq ($(PLATFORM),WSL)
test: test-crypto-win
else ifeq ($(PLATFORM),LINUX)
test: test-crypto-linux
else
test: test-crypto-win
endif
