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
    if(ret != 0) {
        std::cerr << "Error while running: " << cmd << std::endl;
    } else {
        std::cout << "Profile set -> " << profile << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <inactivity_seconds> </path/to/device> [more_devices...]" << std::endl;
        return 1;
    }

    // Parse inactivity time from argument
    int inactivitySeconds = std::stoi(argv[1]);
    auto inactivityInterval = std::chrono::seconds(inactivitySeconds);

    // Variable to store the last non-power-saver profile
    std::string previousProfile = get_current_profile();
    if (previousProfile.empty() || previousProfile == "power-saver") {
        previousProfile = "balanced";
    }
    std::cout << "Initial profile: " << previousProfile << std::endl;

    // Open input device file descriptors
    std::vector<int> fds;
    std::vector<std::string> devNames;
    for (int i = 2; i < argc; ++i) {
        int fd = open(argv[i], O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            std::cerr << "Cannot open " << argv[i] << ": " << strerror(errno) << std::endl;
            continue;
        }
        fds.push_back(fd);
        devNames.push_back(argv[i]);
        std::cout << "Device monitoring: " << argv[i] << std::endl;
    }
    if (fds.empty()) {
        std::cerr << "Error: no device could be opened." << std::endl;
        return 1;
    }

    auto lastActive = std::chrono::steady_clock::now();
    bool inPowerSaver = false;

    std::cout << "\nActivated. Waiting for activity... (" << inactivitySeconds << "s inactivity threshold)\n";

    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = -1;
        for (int fd : fds) {
            FD_SET(fd, &readfds);
            if (fd > max_fd)
                max_fd = fd;
        }

        // Set a timeout for select() of 10 second
        timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        int ret = select(max_fd + 1, &readfds, nullptr, nullptr, &tv);
        if (ret < 0) {
            std::cerr << "Error in select: " << strerror(errno) << std::endl;
            break;
        }

        bool eventOccurred = false;
        if (ret > 0) {
            for (size_t i = 0; i < fds.size(); ++i) {
                if (FD_ISSET(fds[i], &readfds)) {
                    input_event ev;
                    ssize_t n = read(fds[i], &ev, sizeof(ev));
                    if (n == sizeof(ev)) {
                        // Check for key, mouse, or touch events
                        if (ev.type == EV_KEY || ev.type == EV_REL || ev.type == EV_ABS) {
                            eventOccurred = true;
                        }
                    }
                }
            }
        }

        // On activity: update last active time and refresh previousProfile
        if (eventOccurred) {
            lastActive = std::chrono::steady_clock::now();
            // Poll the current profile; if it's not power-saver, update previousProfile
            std::string currentProfile = get_current_profile();
            if (!currentProfile.empty() && currentProfile != "power-saver") {
                previousProfile = currentProfile;
            }
            if (inPowerSaver) {
                // Restore the previously recorded profile
                set_profile(previousProfile);
                inPowerSaver = false;
            }
        } else {
            // Check for inactivity
            auto now = std::chrono::steady_clock::now();
            if (!inPowerSaver && (now - lastActive >= inactivityInterval)) {
                // Before switching, update previousProfile in case it was changed manually
                std::string currentProfile = get_current_profile();
                if (!currentProfile.empty() && currentProfile != "power-saver") {
                    previousProfile = currentProfile;
                }
                set_profile("power-saver");
                inPowerSaver = true;
            }
        }
    }

    // Close input device file descriptors
    for (int fd : fds) {
        close(fd);
    }
    return 0;
}

