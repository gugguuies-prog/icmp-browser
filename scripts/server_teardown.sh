#!/bin/bash
###############################################################################
# server_teardown.sh
# 
# ICMP Tunnel Server Teardown Script
# 
# Undoes all changes made by server_setup.sh:
# - Removes iptables rules
# - Re-enables ICMP echo replies
# - Optionally disables IP forwarding
# - Removes TUN interface
# 
# Usage: sudo ./scripts/server_teardown.sh [tun_interface]
# Example: sudo ./scripts/server_teardown.sh tun0
###############################################################################

# Default configuration
TUN_INTERFACE="${1:-tun0}"
TUN_NETWORK="10.0.1.0/24"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "╔════════════════════════════════════════════════════════════╗"
echo "║        ICMP Tunnel Server Teardown                         ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}ERROR: This script must be run as root${NC}"
    echo "Usage: sudo $0 [tun_interface]"
    exit 1
fi

echo -e "${GREEN}Configuration:${NC}"
echo "  TUN Interface: $TUN_INTERFACE"
echo "  TUN Network:   $TUN_NETWORK"
echo ""

# Step 1: Remove iptables NAT rules
echo -e "${YELLOW}[1/4]${NC} Removing iptables NAT rules..."

# Get default network interface
DEFAULT_IFACE=$(ip route | grep default | awk '{print $5}' | head -n1)
if [ -n "$DEFAULT_IFACE" ]; then
    # Remove MASQUERADE rule
    iptables -t nat -D POSTROUTING -s $TUN_NETWORK -o $DEFAULT_IFACE -j MASQUERADE 2>/dev/null
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}  ✓ NAT MASQUERADE rule removed${NC}"
    else
        echo -e "${YELLOW}  Note: NAT rule not found (may already be removed)${NC}"
    fi
    
    # Remove forwarding rules
    iptables -D FORWARD -i $TUN_INTERFACE -o $DEFAULT_IFACE -j ACCEPT 2>/dev/null
    iptables -D FORWARD -i $DEFAULT_IFACE -o $TUN_INTERFACE -m state --state RELATED,ESTABLISHED -j ACCEPT 2>/dev/null
    echo -e "${GREEN}  ✓ Forward rules removed${NC}"
else
    echo -e "${YELLOW}  WARNING: Could not detect default interface${NC}"
fi

# Step 2: Re-enable ICMP echo replies
echo -e "${YELLOW}[2/4]${NC} Re-enabling ICMP echo replies..."
echo 0 > /proc/sys/net/ipv4/icmp_echo_ignore_all
if [ $? -eq 0 ]; then
    echo -e "${GREEN}  ✓ ICMP auto-replies re-enabled${NC}"
else
    echo -e "${YELLOW}  WARNING: Could not re-enable ICMP auto-replies${NC}"
fi

# Step 3: Optionally disable IP forwarding (commented out by default)
# echo -e "${YELLOW}[3/4]${NC} Disabling IP forwarding..."
# echo 0 > /proc/sys/net/ipv4/ip_forward
# echo -e "${GREEN}  ✓ IP forwarding disabled${NC}"
echo -e "${YELLOW}[3/4]${NC} IP forwarding..."
echo "  Note: Leaving IP forwarding enabled (may be needed by other services)"
echo "  Current value: $(cat /proc/sys/net/ipv4/ip_forward)"

# Step 4: Remove TUN interface (if exists)
echo -e "${YELLOW}[4/4]${NC} Removing TUN interface..."
if ip link show $TUN_INTERFACE &>/dev/null; then
    ip link set $TUN_INTERFACE down 2>/dev/null
    ip link delete $TUN_INTERFACE 2>/dev/null
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}  ✓ TUN interface removed${NC}"
    else
        echo -e "${YELLOW}  Note: TUN interface will be removed when application stops${NC}"
    fi
else
    echo "  TUN interface not found (may already be removed)"
fi

echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║           Server Teardown Complete!                        ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
echo -e "${GREEN}✓ Server configuration cleaned up${NC}"
echo ""

exit 0
