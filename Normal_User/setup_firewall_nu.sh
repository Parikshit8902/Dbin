#!/bin/bash

echo "---------------------------------------------------------------------"
echo "WARNING:"
echo "This script will open the following UDP and TCP ports in the firewall:"
echo "  UDP: 8100, 8103, 8106, 8113"
echo "  TCP: 9000"
echo "These are required for the Normal User program to communicate."
echo ""
echo "The script does NOT track the previous state of these ports."
echo "If these ports were already open for other reasons, they will remain open."
echo "If they were closed, they will be opened."
echo "---------------------------------------------------------------------"
echo ""

read -p "Do you want to apply these firewall rules? (Y/N): " confirm

# Convert input to uppercase for case-insensitive comparison
confirm_upper=$(echo "$confirm" | tr '[:lower:]' '[:upper:]')

if [[ "$confirm_upper" == "Y" ]]; then
    echo "Applying firewall rules for Normal User..."

    # Allow UDP ports
    sudo ufw allow 8100/udp comment 'NU: Receive IP Table from SU'
    sudo ufw allow 8103/udp comment 'NU: Receive files from SU'
    sudo ufw allow 8106/udp comment 'NU: Receive files from other NU'
    sudo ufw allow 8113/udp comment 'NU: Receive replies from CR'

    # Allow TCP port for file transfers
    sudo ufw allow 9000/tcp comment 'NU: Receive TCP file transfers'

    # Enable the firewall and show status
    sudo ufw enable
    sudo ufw reload
    echo "Firewall rules updated for Normal User."
    sudo ufw status verbose
    exit 0
else
    echo "Aborted. No firewall changes were made."
    exit 1
fi
