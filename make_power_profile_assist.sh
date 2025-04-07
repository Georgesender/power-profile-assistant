#!/bin/bash

# Service name
SERVICE_NAME=power-profile-assistant
EXECUTABLE_PATH=/usr/local/bin/power_profile_assistant

# Check if the script is run as root
if [[ $EUID -ne 0 ]]; then
   echo "Please run this script as root."
   exit 1
fi

# Prompt user for input
read -p "Enter idle time (in seconds): " IDLE_TIMEOUT
read -p "Enter the path(s) to input devices (space separated): " -a INPUT_DEVICES

# Validate inputs
if [[ -z "$IDLE_TIMEOUT" || ${#INPUT_DEVICES[@]} -eq 0 ]]; then
    echo "Error: You must specify the idle time and at least one device."
    exit 1
fi

# Form the arguments string for the executable
ARGS="$IDLE_TIMEOUT ${INPUT_DEVICES[*]}"

# Path to the systemd service file
SERVICE_FILE=/etc/systemd/system/${SERVICE_NAME}.service

# Create systemd unit file
cat > "$SERVICE_FILE" <<EOF
[Unit]
Description=Power Profile Auto Switcher
After=network.target

[Service]
ExecStart=$EXECUTABLE_PATH $ARGS
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

# Set appropriate permissions for the service file
chmod 644 "$SERVICE_FILE"

# Reload systemd, enable and start the service
systemctl daemon-reexec
systemctl daemon-reload
systemctl enable --now $SERVICE_NAME

echo "✅ Service $SERVICE_NAME has been created and started!"
echo "ℹ️  The service will now start automatically on system boot."

