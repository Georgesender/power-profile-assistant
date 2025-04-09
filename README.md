# Power Profile Assistant

This is a system service that automatically switches the power profile of a Linux machine based on user activity.

## How it works

- The service continuously monitors input devices (e.g., keyboard, mouse, touchpad).
- After a specified period of inactivity (e.g., 60 seconds), it automatically switches the power profile to **power-saver**.
- When activity resumes, it reverts to the previously active power profile—**only** if the **power-saver** mode was enabled automatically.
- If you change the profile manually while the system is idle or in power-saver mode, the service respects your manual selection.

## Installation

1. **Download the repository:**

   ```bash
   git clone https://github.com/Georgesender/power-profile-assistant.git
   cd power-profile-assistant

2. **Build the executable:**
    
- Compile the source code using g++
    
    ```bash
    g++ -o power_profile_assistant power_profile_assistant.cpp

- Move the executable to /usr/local/bin (you might need root privileges)

    ```bash
    sudo mv power_profile_assistant /usr/local/bin/

3. **Install the service:**
    
    ```bash
    ./create_power_daemon_service.sh
    
The script will:

- Prompt you to enter the idle timeout (in seconds).

- Prompt you for the path(s) to the input devices you want to monitor.

- You can see list of devices:

    ```bash
    sudo libinput list-devices

- Create a systemd service file at /etc/systemd/system/power-profile-assistant.service.

- Reload systemd and enable/start the service.

4. **Checking the Service Status**

To check logs and service status, you can use:

    ```bash
    sudo systemctl status power-profile-assistant
    sudo journalctl -u power-profile-assistant -f

## Uninstallation

To disable and remove the service, run:

    ```bash
    sudo systemctl stop power-profile-assistant
    sudo systemctl disable power-profile-assistant
    sudo rm /etc/systemd/system/power-profile-assistant.service
    sudo systemctl daemon-reload

## Customization:

- Idle Timeout: Adjust the idle timeout when running the install script.

- Input Devices: Specify one or more input device paths (e.g., /dev/input/event0 /dev/input/event1).

- Feel free to fork and modify the repository according to your needs.

- Happy power saving!
