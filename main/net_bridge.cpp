#include "net_bridge.hpp"

#include <cstring>
#include <mutex>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "grbl_link.hpp"
#include "lwip/sockets.h"
#include "sdkconfig.h"

static const char *TAG = "net";

namespace net {

static std::mutex s_mx;
static Info s_info;
static int s_client_fd = -1;
static std::mutex s_send_mx;

Info info() {
    std::lock_guard<std::mutex> g(s_mx);
    return s_info;
}

static void on_wifi(void *, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        {
            std::lock_guard<std::mutex> g(s_mx);
            s_info.wifi_up = false;
            s_info.ip.clear();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto *ev = (ip_event_got_ip_t *)data;
        char buf[16];
        snprintf(buf, sizeof(buf), IPSTR, IP2STR(&ev->ip_info.ip));
        std::lock_guard<std::mutex> g(s_mx);
        s_info.wifi_up = true;
        s_info.ip = buf;
        ESP_LOGI(TAG, "IP %s", buf);
    }
}

// grblHAL output -> LightBurn
static void host_write(const uint8_t *data, size_t len) {
    std::lock_guard<std::mutex> g(s_send_mx);
    if (s_client_fd < 0) return;
    while (len > 0) {
        int n = send(s_client_fd, data, len, 0);
        if (n <= 0) return;  // the server task notices the drop on recv
        data += n;
        len -= (size_t)n;
    }
}

static bool allowed(const sockaddr_in &peer) {
    const char *allow = CONFIG_K40_ALLOWED_IP;
    if (allow[0] == '\0') return true;
    char ip[16];
    inet_ntoa_r(peer.sin_addr, ip, sizeof(ip));
    return strcmp(ip, allow) == 0;
}

static void server_task(void *) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    int yes = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CONFIG_K40_TELNET_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listen_fd, (sockaddr *)&addr, sizeof(addr)) != 0 || listen(listen_fd, 1) != 0) {
        ESP_LOGE(TAG, "cannot listen on port %d", CONFIG_K40_TELNET_PORT);
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGI(TAG, "LightBurn server on port %d", CONFIG_K40_TELNET_PORT);

    uint8_t buf[512];
    while (true) {
        sockaddr_in peer = {};
        socklen_t plen = sizeof(peer);
        int fd = accept(listen_fd, (sockaddr *)&peer, &plen);
        if (fd < 0) continue;
        if (!allowed(peer)) {
            ESP_LOGW(TAG, "refused connection from a non-allowed IP");
            close(fd);
            continue;
        }
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
        {
            std::lock_guard<std::mutex> g(s_send_mx);
            s_client_fd = fd;
        }
        {
            std::lock_guard<std::mutex> g(s_mx);
            s_info.client = true;
        }
        grbl::host_attach(host_write);

        // One LightBurn at a time: serve this client until it drops
        while (true) {
            int n = recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) break;
            grbl::host_rx(buf, (size_t)n);
        }

        grbl::host_attach(nullptr);
        {
            std::lock_guard<std::mutex> g(s_send_mx);
            s_client_fd = -1;
        }
        {
            std::lock_guard<std::mutex> g(s_mx);
            s_info.client = false;
        }
        close(fd);
    }
}

void init() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(netif, CONFIG_K40_HOSTNAME);

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, nullptr));

    wifi_config_t wc = {};
    strncpy((char *)wc.sta.ssid, CONFIG_K40_WIFI_SSID, sizeof(wc.sta.ssid));
    strncpy((char *)wc.sta.password, CONFIG_K40_WIFI_PASSWORD, sizeof(wc.sta.password));
    wc.sta.threshold.authmode = CONFIG_K40_WIFI_PASSWORD[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);  // power save adds latency spikes to the stream

    xTaskCreatePinnedToCore(server_task, "lb_srv", 6144, nullptr, 5, nullptr, 0);
}

}  // namespace net
