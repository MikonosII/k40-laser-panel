#include "web.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "esp_http_server.h"
#include "esp_log.h"
#include "jobs.hpp"
#include "lwip/sockets.h"
#include "settings.hpp"

static const char *TAG = "web";

namespace web {

// One small page: pick a file saved from LightBurn ("Save GCode"), upload it, see what's stored.
// Jobs are started only from the panel, so nobody fires the laser from another room.
static const char PAGE[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>K40 jobs</title>
<style>
:root{--bg:#0F1113;--card:#1A1D21;--line:#343941;--tx:#E8E6E1;--sub:#9A978F;--amb:#F5A524;--red:#FF9A9A;--grn:#7BE0A0}
body{margin:0;background:var(--bg);color:var(--tx);font:16px/1.4 system-ui,sans-serif}
main{max-width:640px;margin:0 auto;padding:16px;display:flex;flex-direction:column;gap:12px}
h1{font-size:22px;margin:4px 0}
.card{background:var(--card);border-radius:10px;padding:14px;display:flex;flex-direction:column;gap:10px}
.sub{color:var(--sub);font-size:14px}
button,label.btn{background:#23272D;color:var(--tx);border:1px solid var(--line);border-radius:8px;padding:10px 14px;font-size:16px;cursor:pointer}
label.btn{display:inline-block;text-align:center}
.pri{background:var(--amb);color:#2A1D04;border:none;font-weight:600}
progress{width:100%;height:14px;accent-color:var(--amb)}
ul{list-style:none;margin:0;padding:0}
li{display:flex;align-items:center;gap:10px;padding:10px 0;border-top:1px solid #262A30}
li:first-child{border-top:none}
li span.n{flex:1;word-break:break-all}
.del{padding:6px 10px;font-size:14px;color:var(--red)}
#msg{min-height:1.4em}
</style></head><body><main>
<h1>K40 laser jobs</h1>
<div class="card">
 <div id="store" class="sub">Checking storage...</div>
 <div class="sub">In LightBurn, click <b>Save GCode</b>, then upload the file here. Start it from the panel's Jobs screen.</div>
 <label class="btn">Choose G-code file<input id="file" type="file" accept=".gc,.gcode,.nc,.ngc,.txt" hidden></label>
 <div id="pick" class="sub">No file chosen</div>
 <button id="up" class="pri" disabled>Upload</button>
 <progress id="bar" max="100" value="0" hidden></progress>
 <div id="msg" class="sub"></div>
</div>
<div class="card"><div class="sub">On the panel</div><div id="run" class="sub"></div><ul id="list"></ul></div>
</main>
<script>
const $=id=>document.getElementById(id);
const mb=b=>(b/1048576).toFixed(b<10485760?1:0)+' MB';
$('file').onchange=()=>{const f=$('file').files[0];$('pick').textContent=f?f.name+' - '+mb(f.size):'No file chosen';$('up').disabled=!f};
$('up').onclick=()=>{
 const f=$('file').files[0]; if(!f) return;
 const x=new XMLHttpRequest(); x.open('POST','/api/upload?name='+encodeURIComponent(f.name));
 $('bar').hidden=false; $('up').disabled=true; $('msg').textContent='Uploading...';
 x.upload.onprogress=e=>{if(e.lengthComputable)$('bar').value=e.loaded*100/e.total};
 x.onload=()=>{$('msg').textContent=x.status==200?'Uploaded '+f.name:'Upload failed: '+x.responseText;$('bar').hidden=true;$('up').disabled=false;load()};
 x.onerror=()=>{$('msg').textContent='Upload failed - lost connection';$('bar').hidden=true;$('up').disabled=false};
 x.send(f);
};
async function del(n){if(!confirm('Delete '+n+'?'))return;
 const r=await fetch('/api/delete?name='+encodeURIComponent(n),{method:'POST'});
 if(!r.ok)$('msg').textContent=await r.text(); load();}
async function load(){
 try{const j=await (await fetch('/api/jobs')).json();
  $('store').textContent=j.sd?'SD card: '+mb(j.free)+' free':'No SD card - files are kept in memory until the panel restarts ('+mb(j.free)+' free)';
  $('run').textContent=j.run?j.run:'';
  const ul=$('list'); ul.innerHTML='';
  if(!j.jobs.length){ul.innerHTML='<li class="sub">No jobs yet</li>'}
  for(const it of j.jobs){const li=document.createElement('li');
   li.innerHTML='<span class="n"></span><span class="sub"></span><button class="del">Delete</button>';
   li.children[0].textContent=it.n; li.children[1].textContent=mb(it.s); li.children[2].onclick=()=>del(it.n); ul.appendChild(li)}
 }catch(e){$('store').textContent='Panel not responding'}}
load(); setInterval(load,3000);
</script></body></html>)HTML";

static bool allowed(httpd_req_t *req) {
    std::string allow = settings::get().allowed_ip;  // same allowlist as LightBurn's port
    if (allow.empty()) return true;
    sockaddr_storage peer = {};
    socklen_t len = sizeof(peer);
    if (getpeername(httpd_req_to_sockfd(req), (sockaddr *)&peer, &len) != 0) return false;
    char ip[48] = {};
    if (peer.ss_family == AF_INET) {
        inet_ntoa_r(((sockaddr_in *)&peer)->sin_addr, ip, sizeof(ip));
    } else {
#if LWIP_IPV6
        const sockaddr_in6 *a6 = (const sockaddr_in6 *)&peer;
        // IPv4 clients arrive as ::ffff:a.b.c.d on a dual-stack socket
        const uint8_t *b = a6->sin6_addr.s6_addr;
        snprintf(ip, sizeof(ip), "%u.%u.%u.%u", b[12], b[13], b[14], b[15]);
#endif
    }
    return allow == ip;
}

static esp_err_t refuse(httpd_req_t *req) {
    httpd_resp_set_status(req, "403 Forbidden");
    return httpd_resp_sendstr(req, "Not allowed from this computer (see Setup > LightBurn computer)");
}

static esp_err_t fail(httpd_req_t *req, const char *status, const std::string &msg) {
    httpd_resp_set_status(req, status);
    return httpd_resp_sendstr(req, msg.c_str());
}

static std::string url_decode(const char *s) {
    std::string out;
    for (; *s; ++s) {
        if (*s == '%' && s[1] && s[2]) {
            char hex[3] = {s[1], s[2], 0};
            out.push_back((char)strtol(hex, nullptr, 16));
            s += 2;
        } else {
            out.push_back(*s == '+' ? ' ' : *s);
        }
    }
    return out;
}

static std::string query_name(httpd_req_t *req) {
    char q[256], v[200];
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) != ESP_OK) return "";
    if (httpd_query_key_value(q, "name", v, sizeof(v)) != ESP_OK) return "";
    return url_decode(v);
}

static esp_err_t page_get(httpd_req_t *req) {
    if (!allowed(req)) return refuse(req);
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, PAGE, sizeof(PAGE) - 1);
}

static esp_err_t jobs_get(httpd_req_t *req) {
    if (!allowed(req)) return refuse(req);
    jobs::StorageInfo si = jobs::storage();
    jobs::Progress p = jobs::progress();
    std::string run;
    if (p.state == jobs::RunState::Running) {
        char b[160];
        snprintf(b, sizeof(b), "Running %s - %u%%", p.name.c_str(),
                 (unsigned)(p.total ? (uint64_t)p.sent * 100 / p.total : 0));
        run = b;
    }
    std::string js = "{\"sd\":";
    js += si.sd ? "true" : "false";
    js += ",\"free\":" + std::to_string(si.free) + ",\"run\":\"" + run + "\",\"jobs\":[";
    bool first = true;
    for (const jobs::Info &j : jobs::list()) {
        if (!first) js += ',';
        first = false;
        js += "{\"n\":\"" + j.name + "\",\"s\":" + std::to_string(j.size) + "}";  // names are pre-cleaned
    }
    js += "]}";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, js.data(), js.size());
}

static esp_err_t upload_post(httpd_req_t *req) {
    if (!allowed(req)) return refuse(req);
    std::string name = query_name(req);
    if (name.empty()) return fail(req, "400 Bad Request", "Missing file name");
    if (req->content_len == 0) return fail(req, "400 Bad Request", "Empty file");
    std::string err;
    jobs::Upload *u = jobs::upload_begin(name, (uint32_t)req->content_len, err);
    if (!u) return fail(req, "507 Insufficient Storage", err);

    static char buf[8192];
    size_t left = req->content_len;
    int timeouts = 0;
    bool ok = true;
    while (left > 0) {
        int n = httpd_req_recv(req, buf, left < sizeof(buf) ? left : sizeof(buf));
        if (n == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < 5) continue;
        if (n <= 0) {
            ok = false;
            break;
        }
        timeouts = 0;
        if (!jobs::upload_write(u, buf, (size_t)n)) {
            ok = false;
            err = "Write failed (card full or removed?)";
            break;
        }
        left -= (size_t)n;
    }
    if (!jobs::upload_end(u, ok)) {
        return fail(req, "500 Internal Server Error", err.empty() ? "Upload incomplete" : err);
    }
    return httpd_resp_sendstr(req, "OK");
}

static esp_err_t delete_post(httpd_req_t *req) {
    if (!allowed(req)) return refuse(req);
    std::string name = query_name(req);
    if (!jobs::remove(name)) return fail(req, "409 Conflict", "Can't delete (running, or already gone)");
    return httpd_resp_sendstr(req, "OK");
}

void init() {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    cfg.recv_wait_timeout = 10;
    cfg.send_wait_timeout = 10;
    cfg.lru_purge_enable = true;
    cfg.core_id = 0;
    httpd_handle_t srv = nullptr;
    if (httpd_start(&srv, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "web server failed to start");
        return;
    }
    const httpd_uri_t uris[] = {
        {"/", HTTP_GET, page_get, nullptr},
        {"/api/jobs", HTTP_GET, jobs_get, nullptr},
        {"/api/upload", HTTP_POST, upload_post, nullptr},
        {"/api/delete", HTTP_POST, delete_post, nullptr},
    };
    for (const httpd_uri_t &u : uris) httpd_register_uri_handler(srv, &u);
    ESP_LOGI(TAG, "job upload page on port 80");
}

}  // namespace web
