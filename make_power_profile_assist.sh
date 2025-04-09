#!/bin/bash

# Service configuration
SERVICE_NAME=power-profile-assistant
EXECUTABLE_PATH=/usr/local/bin/power_profile_assistant

# Check if the script is run as root
if [[ $EUID -ne 0 ]]; then
    echo "🚫 Please run this script as root."
    exit 1
fi

# Check if systemd is present
if ! pidof systemd > /dev/null; then
    echo "🚫 This system does not appear to use systemd."
    exit 1
fi

# Check if the executable exists and is executable
if [[ ! -x "$EXECUTABLE_PATH" ]]; then
    echo "🚫 Executable not found at $EXECUTABLE_PATH or it is not executable."
    exit 1
fi

# Prompt the user for input parameters
read -p "⏱️  Enter idle timeout (in seconds): " IDLE_TIMEOUT
read -p "🖱️  Enter the path(s) to input device(s) (space separated): " -a INPUT_DEVICES

# Validate the required inputs
if [[ -z "$IDLE_TIMEOUT" || ${#INPUT_DEVICES[@]} -eq 0 ]]; then
    echo "🚫 You must specify an idle timeout and at least one input device."
    exit 1
fi

# Form the argument string with proper escaping of device paths
ARGS="$IDLE_TIMEOUT"
for dev in "${INPUT_DEVICES[@]}"; do
    ARGS+=" $(printf '%q' "$dev")"
done

# Define the systemd service file path
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"

# Create the systemd unit file
cat > "$SERVICE_FILE" <<EOF
[Unit]
Description=Power Profile Auto Switcher
After=multi-user.target

[Service]
ExecStart=$EXECUTABLE_PATH $ARGS
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

# Set proper permissions on the service file
chmod 644 "$SERVICE_FILE"

# Reload systemd and enable the service immediately
systemctl daemon-reload
systemctl enable --now "$SERVICE_NAME"

echo -e "\n✅ Service \e[1m$SERVICE_NAME\e[0m has been created and started!"
echo -e "ℹ️  You can check logs using: \e[3mjournalctl -u $SERVICE_NAME -f\e[0m"
