// G-code jobs the panel runs by itself.
//
// Files arrive over Wi-Fi (web.cpp: LightBurn "Save GCode", then the panel's upload page)
// and live on the panel's microSD card in /sdcard/jobs. With no card, they're kept in
// PSRAM instead and are lost at restart. The panel streams the chosen file to grblHAL,
// so a Wi-Fi drop or a sleeping PC can't interrupt a running job.
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace jobs {

struct Info {
    std::string name;
    uint32_t size = 0;
};

struct StorageInfo {
    bool sd = false;       // false = jobs live in memory until restart
    uint64_t free = 0;     // bytes available for uploads
    uint64_t total = 0;
};

enum class RunState : uint8_t { Idle, Running, Done, Stopped, Failed };

struct Progress {
    RunState state = RunState::Idle;
    std::string name;
    uint32_t sent = 0, total = 0;  // file bytes handed to grblHAL
    uint32_t lines = 0, errors = 0;
    uint32_t elapsed_s = 0;
    std::string msg;  // why it stopped or failed
};

void init();  // call after net::init(); mounts the card if one is there

StorageInfo storage();  // checks for a newly inserted card at most every few seconds
std::vector<Info> list();
uint32_t revision();  // changes whenever the list does
bool remove(const std::string &name);

// Feed a stored job to fn in chunks (for the preview). fn returns false to stop early.
// One reader at a time; the job can't be deleted or replaced while it's being read.
bool read_job(const std::string &name, const std::function<bool(const char *, size_t)> &fn);

// Uploads (one at a time). Names are cleaned to letters, digits, space . - _ ( )
struct Upload;
Upload *upload_begin(const std::string &name, uint32_t size, std::string &err);
bool upload_write(Upload *u, const void *data, size_t len);
bool upload_end(Upload *u, bool keep);  // keep=false discards a partial file

bool start(const std::string &name, std::string &err);
void stop();  // soft-resets grblHAL (laser off) and ends the job
Progress progress();
bool running();

}  // namespace jobs
