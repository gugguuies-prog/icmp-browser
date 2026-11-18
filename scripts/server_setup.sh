#!/bin/bash
###############################################################################
# server_setup.sh
# 
# ICMP Tunnel Server Setup Script
# 
# Configures Linux server for ICMP tunneling:
# - Creates TUN interface
# - Enables IP forwarding
# - Configures NAT/MASQUERADE with iptables
# - Disables automatic ICMP replies
# 
# Usage: sudo ./scripts/server_setup.sh [tun_interface] [tun_ip] [tun_netmask]
# Example: sudo ./scripts/server_setup.sh tun0 10.0.1.1 255.255.255.0
###############################################################################

# Default configuration
TUN_INTERFACE="${1:-tun0}"
TUN_IP="${2:-10.0.1.1}"
TUN_NETMASK="${3:-255.255.255.0}"
TUN_NETWORK="10.0.1.0/24"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "╔════════════════════════════════════════════════════════════╗"
echo "║        ICMP Tunnel Server Setup                            ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}ERROR: This script must be run as root${NC}"
    echo "Usage: sudo $0 [tun_interface] [tun_ip] [tun_netmask]"
    exit 1
fi

echo -e "${GREEN}Configuration:${NC}"
echo "  TUN Interface: $TUN_INTERFACE"
echo "  TUN IP:        $TUN_IP"
echo "  TUN Netmask:   $TUN_NETMASK"
echo "  TUN Network:   $TUN_NETWORK"
echo ""

# Step 1: Check if TUN/TAP module is loaded
echo -e "${YELLOW}[1/6]${NC} Checking TUN/TAP kernel module..."
if ! lsmod | grep -q tun; then
    echo "  Loading tun module..."
    modprobe tun
    if [ $? -ne 0 ]; then
        echo -e "${RED}  ERROR: Failed to load tun module${NC}"
        exit 1
    fi
fi
echo -e "${GREEN}  ✓ TUN module loaded${NC}"

# Step 2: Enable IP forwarding
echo -e "${YELLOW}[2/6]${NC} Enabling IP forwarding..."
echo 1 > /proc/sys/net/ipv4/ip_forward
if [ $? -eq 0 ]; then
    echo -e "${GREEN}  ✓ IP forwarding enabled${NC}"
else
    echo -e "${RED}  ERROR: Failed to enable IP forwarding${NC}"
    exit 1
fi

# Make it persistent (optional)
if ! grep -q "^net.ipv4.ip_forward=1" /etc/sysctl.conf 2>/dev/null; then
    echo "net.ipv4.ip_forward=1" >> /etc/sysctl.conf
    echo "  ✓ Made persistent in /etc/sysctl.conf"
fi

# Step 3: Disable ICMP echo replies (to avoid conflicts)
echo -e "${YELLOW}[3/6]${NC} Disabling automatic ICMP echo replies..."
echo 1 > /proc/sys/net/ipv4/icmp_echo_ignore_all
if [ $? -eq 0 ]; then
    echo -e "${GREEN}  ✓ ICMP auto-replies disabled${NC}"
else
    echo -e "${YELLOW}  WARNING: Could not disable ICMP auto-replies${NC}"
fi

# Step 4: Configure NAT/MASQUERADE with iptables
echo -e "${YELLOW}[4/6]${NC} Configuring iptables NAT..."

# Get default network interface (for internet)
DEFAULT_IFACE=$(ip route | grep default | awk '{print $5}' | head -n1)
if [ -z "$DEFAULT_IFACE" ]; then
    echo -e "${RED}  ERROR: Could not detect default network interface${NC}"
    exit 1
fi
echo "  Default interface: $DEFAULT_IFACE"

# Enable MASQUERADE for TUN network
iptables -t nat -A POSTROUTING -s $TUN_NETWORK -o $DEFAULT_IFACE -j MASQUERADE
if [ $? -eq 0 ]; then
    echo -e "${GREEN}  ✓ NAT MASQUERADE configured${NC}"
else
    echo -e "${RED}  ERROR: Failed to configure NAT${NC}"
    exit 1
fi

# Allow forwarding from TUN to default interface
iptables -A FORWARD -i $TUN_INTERFACE -o $DEFAULT_IFACE -j ACCEPT
iptables -A FORWARD -i $DEFAULT_IFACE -o $TUN_INTERFACE -m state --state RELATED,ESTABLISHED -j ACCEPT

echo -e "${GREEN}  ✓ Forwarding rules added${NC}"

# Step 5: Configure TUN interface (will be done by application)
echo -e "${YELLOW}[5/6]${NC} TUN interface configuration..."
echo "  Note: TUN interface will be created and configured by the ICMP tunnel application"
echo "  Expected: $TUN_INTERFACE with IP $TUN_IP/$TUN_NETMASK"

# Step 6: Display current configuration
echo -e "${YELLOW}[6/6]${NC} Verifying configuration..."
echo ""
echo "IP Forwarding:"
cat /proc/sys/net/ipv4/ip_forward
echo ""
echo "ICMP Echo Ignore:"
cat /proc/sys/net/ipv4/icmp_echo_ignore_all
echo ""
echo "NAT Rules:"
iptables -t nat -L POSTROUTING -n -v | grep "$TUN_NETWORK"
echo ""
echo "Forward Rules:"
iptables -L FORWARD -n -v | grep "$TUN_INTERFACE"
echo ""

echo "╔════════════════════════════════════════════════════════════╗"
echo "║           Server Setup Complete!                           ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
echo -e "${GREEN}✓ Server is ready for ICMP tunneling${NC}"
echo ""
echo "Next steps:"
echo "  1. Start the ICMP tunnel server application"
echo "  2. Connect client with: ./icmptunnel client <server_ip>"
echo ""
echo "To undo these changes, run: sudo ./scripts/server_teardown.sh"
echo ""

exit 0
