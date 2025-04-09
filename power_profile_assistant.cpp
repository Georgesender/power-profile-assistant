#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <linux/input.h>
#include <csignal>

// Function to get the current profile using powerprofilesctl
std::string get_current_profile() {
    FILE* pipe = popen("powerprofilesctl get", "r");
    if (!pipe) {
        std::cerr << "Unable to retrieve current profile" << std::endl;
        return "";
    }
    char buffer[128];
    std::string result;
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result = buffer;
    }
    pclose(pipe);
    // Remove newline characters
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

// Function to set a profile using powerprofilesctl
void set_profile(const std::string &profile) {
    std::string cmd = "powerprofilesctl set " + profile;
    int ret = system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "Error while running: " << cmd << std::endl;
    } else {
        std::cout << "Profile set -> " << profile << std::endl;
    }
}

// Global flag for proper termination
volatile std::sig_atomic_t stopFlag = 0;
void signal_handler(int signum) {
    stopFlag = 1;
}

int main(int argc, char* argv[]) {
    // Signal handling for graceful termination (SIGINT, SIGTERM)
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <inactivity_seconds> </path/to/device> [more_devices...]" << std::endl;
        return 1;
    }

    // Parse the inactivity interval
    int inactivitySeconds = std::stoi(argv[1]);
    auto inactivityInterval = std::chrono::seconds(inactivitySeconds);

    // Get the initial profile
    std::string previousProfile = get_current_profile();
    // If the retrieved profile is "power-saver" or empty, use "balanced" as default
    if (previousProfile.empty() || previousProfile == "power-saver") {
        previousProfile = "balanced";
    }
    std::cout << "Initial profile: " << previousProfile << std::endl;

    // Open input device file descriptors
    std::vector<int> fds;
    for (int i = 2; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            std::cerr << "Cannot open " << argv[i] << ": " << strerror(errno) << std::endl;
            continue;
        }
        fds.push_back(fd);
        std::cout << "Device monitoring: " << argv[i] << std::endl;
    }
    if (fds.empty()) {
        std::cerr << "Error: no device could be opened." << std::endl;
        return 1;
    }

    auto lastActive = std::chrono::steady_clock::now();
    // Flag to indicate if power-saver was set automatically due to inactivity
    bool autoPowerSaver = false;

    std::cout << "\nActivated. Waiting for activity... (" << inactivitySeconds << "s inactivity threshold)\n";

    auto lastProfileCheck = std::chrono::steady_clock::now();

    // Main loop of the service
    while (!stopFlag) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = -1;
        for (int fd : fds) {
            FD_SET(fd, &readfds);
            if (fd > max_fd)
                max_fd = fd;
        }

        timeval tv;
        tv.tv_sec = 1;  // 1-second timeout for increased responsiveness
        tv.tv_usec = 0;
        int ret = select(max_fd + 1, &readfds, nullptr, nullptr, &tv);

        if (ret < 0) {
            std::cerr << "[Error] Error in select: " << strerror(errno) << std::endl;
            break;
        }

        auto now = std::chrono::steady_clock::now();

        // Periodically check the current profile (every 10 seconds)
        if (now - lastProfileCheck >= std::chrono::seconds(10)) {
            std::string currentProfile = get_current_profile();
            // Update previousProfile only if the profile changed manually
            if (!currentProfile.empty() && currentProfile != "power-saver" && !autoPowerSaver && currentProfile != previousProfile) {
                previousProfile = currentProfile;
                std::cout << "[Debug] [AutoRefresh] Updated previousProfile -> " << previousProfile << std::endl;
            }
            lastProfileCheck = now;
        }

        bool eventOccurred = false;
        if (ret > 0) {
            // Process any pending events from all devices
            for (size_t i = 0; i < fds.size(); ++i) {
                if (FD_ISSET(fds[i], &readfds)) {
                    input_event ev;
                    // Read all pending events for this device
                    while (read(fds[i], &ev, sizeof(ev)) == sizeof(ev)) {
                        // Consider key, relative or absolute events as activity
                        if (ev.type == EV_KEY || ev.type == EV_REL || ev.type == EV_ABS) {
                            eventOccurred = true;
                        }
                    }
                }
            }
        }

        // If activity is detected
        if (eventOccurred) {
            lastActive = now;
            // If power-saver was automatically set due to inactivity, revert to previous profile
            if (autoPowerSaver) {
                set_profile(previousProfile);
                autoPowerSaver = false;
            } else {
                // Update previousProfile if the current profile (possibly changed manually) is not "power-saver"
                std::string currentProfile = get_current_profile();
                if (!currentProfile.empty() && currentProfile != "power-saver") {
                    previousProfile = currentProfile;
                }
            }
        }
        // No activity detected
        else {
            // If there has been inactivity and power-saver was not set automatically yet
            if (!autoPowerSaver && (now - lastActive >= inactivityInterval)) {
                // Check the current profile; if it's not power-saver, update previousProfile and set power-saver
                std::string currentProfile = get_current_profile();
                if (!currentProfile.empty() && currentProfile != "power-saver") {
                    previousProfile = currentProfile;
                    set_profile("power-saver");
                    autoPowerSaver = true;
                }
                // If the profile is already power-saver (manually set), do nothing
            }
        }
    }

    // Close input device file descriptors
    for (int fd : fds) {
        close(fd);
    }

    std::cout << "Service terminated gracefully." << std::endl;
    return 0;
}
