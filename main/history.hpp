// Job history: the last 20 jobs (panel or LightBurn) with when, how long, peak tube current
// and how they ended. Kept in NVS so it survives restarts.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace history {

enum class Result : uint8_t { Done, Stopped, Failed, LightBurn };

struct Entry {
    char name[44];
    uint32_t start;      // Unix time, 0 if the clock wasn't set yet
    uint32_t secs;       // run time
    uint16_t peak_ma10;  // peak tube current x10 (0 = no sensor)
    uint8_t result;      // Result
    uint8_t reserved;
};

void init();
void tick();  // call a few times a second (UI refresh); spots jobs starting and ending
std::vector<Entry> list();  // newest first
uint32_t revision();
std::string describe(const Entry &e);  // "Oct 17 3:02 PM · 12:34 · peak 14.2 mA · Done"

}  // namespace history
