# Power Profile Assistant

This is a system service that automatically switches the power profile of a Linux machine based on user activity.

## How it works

- The service monitors input devices (keyboard, mouse, touchpad).
- If there is no activity for a specified period (default: 60 seconds), the power profile is automatically switched to **power-saver**.
- When activity resumes, the previous power profile is restored.

## Installation

1. **Download the repository:**

   ```bash
   git clone https://github.com/yourusername/power-profile-assistant.git
   cd power-profile-assistant

2. **Build the executable:**
    
    ```bash
    g++ -o power_profile_assistant power_profile_assistant.cpp

3. **Install the service:**
    
    ```bash
    ./create_power_daemon_service.sh
