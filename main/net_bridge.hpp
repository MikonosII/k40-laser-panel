#pragma once
#include <string>

namespace net {

struct Info {
    bool wifi_up = false;
    bool client = false;
    std::string ip;  // panel's own IP when connected
};

// Join Wi-Fi and serve LightBurn on CONFIG_K40_TELNET_PORT.
void init();
Info info();

}  // namespace net
