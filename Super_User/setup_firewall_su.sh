#!/bin/bash

echo "---------------------------------------------------------------------"
echo "WARNING:"
echo "This script will open the following UDP and TCP ports in the firewall:"
echo "  UDP: 8102, 8108, 8111"
echo "  TCP: 9000"
echo "These are required for the Super User program to communicate."
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
    echo "Applying firewall rules for Super User..."

    # Allow UDP ports
    sudo ufw allow 8102/udp comment 'SU: Receive files from NU'
    sudo ufw allow 8108/udp comment 'SU: Receive fsee reply from CR'
    sudo ufw allow 8111/udp comment 'SU: Receive fback reply from CR'

    # Allow TCP port for file transfers
    sudo ufw allow 9000/tcp comment 'SU: Receive TCP file transfers'

    # Enable the firewall and show status
    sudo ufw enable
    sudo ufw reload
    echo "Firewall rules updated for Super User."
    sudo ufw status verbose
    exit 0
else
    echo "Aborted. No firewall changes were made."
    exit 1
fi
