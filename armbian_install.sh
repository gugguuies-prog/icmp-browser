#!/bin/bash
# Script d'installation du serveur ICMP sur Armbian
# Usage: ./armbian_install.sh

set -e

echo "========================================="
echo " ICMP Tunnel Server - Installation"
echo "========================================="
echo ""

# Check if running as root
if [ "$EUID" -eq 0 ]; then 
   echo "⚠️  Running as root - good!"
else
   echo "⚠️  Not running as root. Will use sudo."
   echo "📝 Please enter sudo password when prompted."
fi

# Update package list
echo "📦 Updating package list..."
sudo apt-get update -qq

# Install dependencies
echo "📦 Installing build dependencies..."
sudo apt-get install -y \
    build-essential \
    gcc \
    g++ \
    make \
    libssl-dev \
    zlib1g-dev \
    linux-libc-dev \
    libc6-dev

echo ""
echo "✅ Dependencies installed!"
echo ""

# Compile server
echo "🔨 Compiling ICMP server..."
cd ~/icmptunnel-build

# Clean previous builds
make clean 2>/dev/null || true

# Build server tests
echo "   Building test_icmp_handler..."
make test-icmp-handler-linux

echo "   Building test_session_manager..."
make test-session-manager-linux

echo "   Building test_tun_device..."
make test-tun-device-linux

echo ""
echo "✅ Server compiled successfully!"
echo ""

# Run quick test
echo "🧪 Running quick validation test..."
if [ -f bin/test_icmp_handler ]; then
    echo "   test_icmp_handler exists ✓"
else
    echo "   ❌ test_icmp_handler missing!"
    exit 1
fi

echo ""
echo "========================================="
echo " ✅ INSTALLATION COMPLETE!"
echo "========================================="
echo ""
echo "📍 Server binary location: ~/icmptunnel-build/bin/"
echo ""
echo "🚀 Next steps:"
echo "   1. Run setup script: sudo ~/icmptunnel-build/scripts/server_setup.sh"
echo "   2. Test server: sudo ~/icmptunnel-build/bin/test_icmp_handler"
echo "   3. Start server: TBD - server main program"
echo ""
echo "📚 Documentation: ~/icmptunnel-build/BUILD.md"
echo ""
