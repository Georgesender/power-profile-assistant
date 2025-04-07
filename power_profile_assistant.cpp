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

// Function to get the current profile with powerprofilesctl
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
    // Delete newline characters
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

    // previous profile
    std::string initialProfile = get_current_profile();
    if (initialProfile.empty()) {
        initialProfile = "balanced";
    }
    if (initialProfile == "power-saver") {
        initialProfile = "balanced";
    }
    std::cout << "Current profile: " << initialProfile << std::endl;

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
        std::cerr << "Bruh... there is no device to open." << std::endl;
        return 1;
    }

    auto lastActive = std::chrono::steady_clock::now();
    bool inPowerSaver = false;

    std::cout << "\nActivated. Waiting actions... (" << inactivitySeconds << "s inactivity threshold)\n";

    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = -1;
        for (int fd : fds) {
            FD_SET(fd, &readfds);
            if (fd > max_fd)
                max_fd = fd;
        }

        timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int ret = select(max_fd + 1, &readfds, nullptr, nullptr, &tv);
        if (ret < 0) {
            std::cerr << "Error select: " << strerror(errno) << std::endl;
            break;
        }

        bool eventOccurred = false;

        if (ret > 0) {
            for (size_t i = 0; i < fds.size(); ++i) {
                if (FD_ISSET(fds[i], &readfds)) {
                    input_event ev;
                    ssize_t n = read(fds[i], &ev, sizeof(ev));
                    if (n == sizeof(ev)) {
                        if (ev.type == EV_KEY || ev.type == EV_REL || ev.type == EV_ABS) {
                            eventOccurred = true;
                        }
                    }
                }
            }
        }

        if (eventOccurred) {
            lastActive = std::chrono::steady_clock::now();
            if (inPowerSaver) {
                set_profile(initialProfile);
                inPowerSaver = false;
            }
        } else {
            auto now = std::chrono::steady_clock::now();
            if (!inPowerSaver && (now - lastActive >= inactivityInterval)) {
                set_profile("power-saver");
                inPowerSaver = true;
            }
        }
    }

    for (int fd : fds) {
        close(fd);
    }
    return 0;
}

