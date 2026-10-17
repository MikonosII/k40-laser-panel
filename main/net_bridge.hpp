#pragma once
#include <string>
#include <vector>

namespace net {

struct Info {
    bool wifi_up = false;
    bool client = false;
    std::string ip;  // panel's own IP when connected
};

// Join Wi-Fi (settings::get()) and serve LightBurn on CONFIG_K40_TELNET_PORT.
void init();
Info info();

// Re-apply Wi-Fi credentials after the user saves new settings.
void reconnect();

// Blocking scan (a few seconds): unique SSIDs, strongest first. Call from a worker task.
std::vector<std::string> scan();

}  // namespace net
