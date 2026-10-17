// Link to grblHAL on the SKR, shared between LightBurn (the "host") and the panel.
//
// Rules that keep LightBurn's stream intact:
//  * Realtime bytes (? ! ~ Ctrl-X, 0x80+) go straight through from either side.
//  * Every G-code line gets exactly one "ok"/"error:" back, in order. We remember
//    who sent each line; replies to panel lines are swallowed, everything else is
//    forwarded to LightBurn.
//  * The panel may only send a line when LightBurn has nothing in flight and no
//    job is running. Host lines that arrive while a panel line is pending are
//    held briefly and released in order.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace grbl {

enum class State : uint8_t { Unknown, Idle, Run, Hold, Jog, Alarm, Door, Check, Home, Sleep, Tool };
const char *state_name(State s);

struct Status {
    State state = State::Unknown;
    bool link_ok = false;  // a status report arrived in the last second
    float mpos[3] = {};
    float wpos[3] = {};
    float feed = 0;     // mm/min
    float spindle = 0;  // S value
    int ov_feed = 100, ov_rapid = 100, ov_spindle = 100;
    bool flood = false, mist = false, spindle_on = false;
    bool door_pin = false;   // Pn:D
    bool hold_pin = false;   // Pn:H  PWR-DET: lid switch (2nd contact) open
    bool probe_pin = false;  // Pn:P  PROBE: 2nd water flow switch open (no flow)
    int alarm = 0;          // ALARM:n while in Alarm state
    bool host_connected = false;
    bool job_active = false;
    uint32_t job_seconds = 0;
    bool panel_busy = false;  // panel line waiting for its reply
};

namespace rt {
constexpr uint8_t STATUS = '?', HOLD = '!', RESUME = '~', RESET = 0x18;
constexpr uint8_t JOG_CANCEL = 0x85;
constexpr uint8_t FEED_RESET = 0x90, FEED_P10 = 0x91, FEED_M10 = 0x92;
constexpr uint8_t SPINDLE_RESET = 0x99, SPINDLE_P10 = 0x9A, SPINDLE_M10 = 0x9B;
constexpr uint8_t FLOOD_TOGGLE = 0xA0, MIST_TOGGLE = 0xA1;
}  // namespace rt

void init();
Status status();
std::vector<std::string> console();  // recent messages, oldest first

// Host (LightBurn) side
using HostWriter = void (*)(const uint8_t *data, size_t len);
void host_attach(HostWriter w);  // nullptr when the client disconnects
void host_rx(const uint8_t *data, size_t len);

// Panel side
void realtime(uint8_t c);
bool panel_can_send();
bool send(const char *line);  // one line, no newline; false if not allowed now
bool send_lines(const char *const *lines, int n);
void panel_stop();  // soft reset, then ignore LightBurn until it goes quiet

// Panel jobs: the panel streams a G-code file itself (see jobs.hpp). While one runs,
// LightBurn's G-code lines get "error:9" and its reset byte is ignored; its status polls,
// pause/resume and overrides still pass through.
enum class SendResult { Sent, Full, Aborted };
bool job_begin();                          // false unless grblHAL is Idle and nobody else is streaming
SendResult job_send(const std::string &line);  // no newline; Full = wait for grblHAL to catch up
bool job_acked();                          // every job line has had its ok/error
bool job_aborted();                        // reset, alarm or panel Stop since job_begin()
uint32_t job_errors();
void job_end();

}  // namespace grbl
