#!/bin/bash

echo "---------------------------------------------------------------------"
echo "WARNING:"
echo "This script will open the following UDP and TCP ports in the firewall:"
echo "  UDP: 8101, 8104, 8107"
echo "  TCP: 9000"
echo "These are required for the Central Repository program to communicate."
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
    echo "Applying firewall rules for Central Repository..."

    # Allow UDP ports
    sudo ufw allow 8101/udp comment 'CR: Receive IP Table from SU'
    sudo ufw allow 8104/udp comment 'CR: Receive commands/files from SU'
    sudo ufw allow 8107/udp comment 'CR: Receive commands/files from NU'

    # Allow TCP port for file transfers
    sudo ufw allow 9000/tcp comment 'CR: Receive TCP file transfers'

    # Enable the firewall and show status
    sudo ufw enable
    sudo ufw reload
    echo "Firewall rules updated for Central Repository."
    sudo ufw status verbose
    exit 0
else
    echo "Aborted. No firewall changes were made."
    exit 1
fi
