#!/bin/bash
# Installation script for ICMP Server on Armbian (Orange Pi)

echo "===================================="
echo "  ICMP Server - Armbian Installation"
echo "===================================="

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run with sudo"
    exit 1
fi

# Install dependencies
echo "Installing dependencies..."
apt-get update
apt-get install -y build-essential libssl-dev zlib1g-dev git

# Create directories
mkdir -p ~/icmp-server-final/bin
cd ~/icmp-server-final

# Pull latest code
if [ -d ".git" ]; then
    echo "Updating code..."
    git pull
else
    echo "Cloning repository..."
    git clone https://github.com/gugguuies-prog/icmp-browser.git .
fi

# Compile server with HTTP proxy
echo "Compiling server..."
gcc -Wall -Wextra -O2 -pthread -Isrc \
  src/server/main.c \
  src/server/icmp_handler.c \
  src/server/session_manager.c \
  src/server/tun_device.c \
  src/server/http_proxy.c \
  src/common/protocol.c \
  src/common/handshake.c \
  src/common/compression.c \
  src/common/priority_queue.c \
  src/common/statistics.c \
  src/crypto/aes_gcm.c \
  src/crypto/diffie_hellman.c \
  -o bin/icmp_server \
  -lcrypto -lssl -lz -lpthread

if [ $? -eq 0 ]; then
    echo ""
    echo "===================================="
    echo "  Compilation successful!"
    echo "===================================="
    echo ""
    echo "To run the server:"
    echo "  sudo sysctl -w net.ipv4.icmp_echo_ignore_all=1"
    echo "  sudo ./bin/icmp_server -i end0 -v"
    echo ""
else
    echo "Compilation failed!"
    exit 1
fi
