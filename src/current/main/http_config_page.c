/*============================================================================
 MODULE: http_config_page

 RESPONSIBILITY
 - Renders GET /config -- configuration form
 - Parses POST /save form body (URL-encoded key=value pairs)
 - Split from http_portal.c v15.11

 REVERT HISTORY
 R0  v15.11  Split from http_portal.c
 R1  v0.1-flex  Add Operating Mode selector (Standalone/NUT Client/Bridge)
                Add Upstream Target section (host + port) with JS show/hide
                Parse op_mode, upstream_host, upstream_port from POST body
 R2  v0.2-flex  Two-column layout: mode description panel on the right
                Mode cards update live when selector changes
 R3  v0.11-flex Renumber modes 1/2/3 (was 0/1/2) to match status page and log output
 R4  v0.34-flex Add UPS Protocol selector (HID / QS Serial) for dual-protocol devices

============================================================================*/

#include "http_config_page.h"
#include "http_portal_css.h"
#include "wifi_mgr.h"
#include "cfg_store.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

#define HTTP_BODY_MAX 2048

static void strlcpy0(char *dst, const char *src, size_t dstsz) {
    if (!dst || dstsz == 0) return;
    if (!src) { dst[0] = 0; return; }
    strlcpy(dst, src, dstsz);
}

static void url_decode_inplace(char *s) {
    if (!s) return;
    char *w = s;
    for (char *r = s; *r; r++) {
        if (*r == '+') {
            *w++ = ' ';
        } else if (*r == '%' && isxdigit((unsigned char)r[1]) && isxdigit((unsigned char)r[2])) {
            char hh[3] = { r[1], r[2], 0 };
            *w++ = (char)strtol(hh, NULL, 16);
            r += 2;
        } else {
            *w++ = *r;
        }
    }
    *w = 0;
}

static void trim_ws(char *s) {
    if (!s) return;
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = 0;
}

void render_config(app_cfg_t *cfg, char *out, size_t outsz,
                   const char *note, const char *note_cls)
{
    char sta_ip[16];
    wifi_mgr_sta_ip_str(sta_ip);
    bool ap_up = wifi_mgr_ap_is_active();

    char note_html[256] = {0};
    if (note && note[0]) {
        const char *cls = (note_cls && strcmp(note_cls, "err") == 0) ? "note-err" : "note-ok";
        snprintf(note_html, sizeof(note_html), "<div class='%s'>%s</div>", cls, note);
    }

    const char *pw_warn = cfg_store_is_default_pass(cfg)
        ? "<div class='warn'>Default password in use. Change it below under Portal Security.</div>"
        : "";

    /* Mode selector state */
    const char *sel_sa  = (cfg->op_mode == OP_MODE_STANDALONE) ? "selected" : "";
    const char *sel_nc  = (cfg->op_mode == OP_MODE_NUT_CLIENT) ? "selected" : "";
    const char *sel_br  = (cfg->op_mode == OP_MODE_BRIDGE)     ? "selected" : "";
    const char *up_disp = (cfg->op_mode != OP_MODE_STANDALONE) ? "" : "none";
    uint16_t    up_port = cfg->upstream_port ? cfg->upstream_port : 3493;

    /* Protocol selector state */
    const char *sel_hid = (cfg->ups_protocol == UPS_PROTO_HID) ? "selected" : "";
    const char *sel_qs  = (cfg->ups_protocol == UPS_PROTO_QS)  ? "selected" : "";

    snprintf(out, outsz,
        "<!doctype html><html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>UPS Node Config</title>"
        PORTAL_CSS

        /* Per-page CSS overrides and two-column layout styles */
        "<style>"
        "body{max-width:none}"
        ".page-wrap{display:flex;gap:28px;align-items:flex-start;max-width:880px}"
        ".mode-info{width:260px;flex-shrink:0;padding-top:2px}"
        ".mode-card{border:1px solid #2a2a2a;border-left:3px solid #4fc3f7;"
                   "padding:16px;background:#0d0d0d}"
        ".mc-tag{color:#4fc3f7;font-family:Arial,sans-serif;font-size:0.72em;"
                "text-transform:uppercase;letter-spacing:0.10em;margin-bottom:8px}"
        ".mc-name{color:#e8e8e2;font-family:Arial,sans-serif;font-size:0.9em;"
                 "font-weight:600;margin-bottom:14px}"
        ".mc-row{color:#777;font-family:Arial,sans-serif;font-size:0.79em;"
                "line-height:1.65;padding:3px 0 3px 10px;"
                "border-left:1px solid #1c1c1c;margin-bottom:3px}"
        ".mc-row b{color:#aaa}"
        ".mc-foot{color:#555;font-family:Arial,sans-serif;font-size:0.73em;"
                 "margin-top:14px;padding-top:10px;"
                 "border-top:1px solid #1c1c1c;line-height:1.65}"
        "@media(max-width:700px){"
          ".page-wrap{flex-direction:column}"
          ".mode-info{width:100%%}"
        "}"
        "</style>"
        "</head><body>"

        "<h2>ESP32-S3 UPS Node</h2>"
        "<div class='subtitle'>v0.1-flex &mdash; Configuration</div>"
        "%s%s"  /* pw_warn, note_html */

        "<div class='page-wrap'>"
        "<div>"  /* left column: form */

        /* ---- Operating Mode ---- */
        "<form method='POST' action='/save' onsubmit='return chkSave()'>"
        "<div class='form-section'>Operating Mode</div>"
        "<div class='form-row'><span class='form-label'>Mode</span>"
            "<select name='op_mode' onchange='onModeChange(this.value)'>"
            "<option value='1' %s>Mode 1 - Standalone - NUT server on device</option>"
            "<option value='2' %s>Mode 2 - NUT Client - push to upstream upsd</option>"
            "<option value='3' %s>Mode 3 - Bridge - forward raw HID stream</option>"
            "</select></div>"

        /* ---- UPS Protocol (dual-protocol devices only) ---- */
        "<div class='form-section'>UPS Protocol</div>"
        "<div class='form-row'><span class='form-label'>Protocol</span>"
            "<select name='ups_protocol'>"
            "<option value='0' %s>HID (default)</option>"
            "<option value='1' %s>QS Serial (Voltronic/Megatec)</option>"
            "</select></div>"
        "<div style='color:#777;font-size:0.79em;margin:4px 0 12px 0;"
             "font-family:Arial,sans-serif'>"
          "HID: standard USB Power Device protocol (most UPS models).<br>"
          "QS Serial: Voltronic/Megatec ASCII protocol (PowerWalker, Phoenixtec).<br>"
          "Only change this if your device supports dual protocols. Requires reboot."
        "</div>"

        /* ---- Wi-Fi STA ---- */
        "<div class='form-section'>Wi-Fi (STA)</div>"
        "<div class='form-row'><span class='form-label'>SSID</span>"
            "<input name='sta_ssid' maxlength='32' value='%s'></div>"
        "<div class='form-row'><span class='form-label'>Password</span>"
            "<input name='sta_pass' type='password' maxlength='64' value='%s'></div>"

        /* ---- Soft AP ---- */
        "<div class='form-section'>Soft AP &mdash; %s</div>"
        "<div class='form-row'><span class='form-label'>AP SSID</span>"
            "<input name='ap_ssid' maxlength='32' value='%s'></div>"
        "<div class='form-row'><span class='form-label'>AP Password (8+ chars)</span>"
            "<input name='ap_pass' type='password' maxlength='64' value='%s'></div>"

        /* ---- NUT Identity ---- */
        "<div class='form-section'>NUT Identity</div>"
        "<div class='form-row'><span class='form-label'>UPS Name</span>"
            "<input name='ups_name' maxlength='32' value='%s'></div>"
        "<div class='form-row'><span class='form-label'>NUT Username</span>"
            "<input name='nut_user' maxlength='32' value='%s'></div>"
        "<div class='form-row'><span class='form-label'>NUT Password</span>"
            "<input name='nut_pass' type='password' maxlength='32' value='%s'></div>"

        /* ---- Upstream Target (shown for NUT Client and Bridge) ---- */
        "<div id='upstream_sec' style='display:%s'>"
        "<div class='form-section'>Upstream Target</div>"
        "<div class='form-row'><span class='form-label'>Host</span>"
            "<input id='upstream_host' name='upstream_host' maxlength='63' value='%s'></div>"
        "<div id='upstream_err' style='display:none;color:#ff5252;"
             "font-size:0.82em;margin:4px 0 8px 0;font-family:Arial,sans-serif'>"
             "Upstream host is required for Mode 2 and Mode 3.</div>"
        "<div class='form-row'><span class='form-label'>Port</span>"
            "<input name='upstream_port' type='number' min='1' max='65535' value='%u'></div>"
        "</div>"

        /* ---- Portal Security ---- */
        "<div class='form-section'>Portal Security &mdash; login: admin / &lt;password&gt;</div>"
        "<div class='form-row'><span class='form-label'>New Password</span>"
            "<input name='portal_pass' type='password' maxlength='32' "
            "placeholder='blank = keep current'></div>"

        "<input class='btn' type='submit' value='Save and Apply'>"
        "</form>"

        "<div class='nav' style='margin-top:20px'>"
        "<a href='/'>Back to Status</a>"
        "<a href='/reboot' onclick=\"return confirm('Reboot device?')\">Reboot</a>"
        "<span style='color:#555;font-size:0.82em'>STA: %s</span>"
        "</div>"

        "</div>"  /* end left column */

        /* ---- Right column: mode description cards ---- */
        "<div class='mode-info'>"

        /* Card 1 - Standalone */
        "<div id='mc0' class='mode-card'>"
        "<div class='mc-tag'>Mode 1</div>"
        "<div class='mc-name'>Standalone NUT Server</div>"
        "<div class='mc-row'><b>Decodes</b> USB HID on the device</div>"
        "<div class='mc-row'><b>Serves</b> NUT protocol on tcp/3493</div>"
        "<div class='mc-row'><b>Clients</b> connect to device IP directly</div>"
        "<div class='mc-row'><b>No upstream</b> infrastructure required</div>"
        "<div class='mc-foot'>"
          "Default mode. Works out of the box with any NUT client "
          "(upsmon, upsc, Home Assistant).<br><br>"
          "Modes 2 and 3 fall back here automatically if the upstream "
          "host is unreachable at boot."
        "</div>"
        "</div>"

        /* Card 2 - NUT Client */
        "<div id='mc1' class='mode-card' style='display:none'>"
        "<div class='mc-tag'>Mode 2</div>"
        "<div class='mc-name'>NUT Client Push</div>"
        "<div class='mc-row'><b>Decodes</b> USB HID on the device</div>"
        "<div class='mc-row'><b>Pushes</b> live data to upstream NUT server</div>"
        "<div class='mc-row'><b>Identity</b> pushed once on connect</div>"
        "<div class='mc-row'><b>State</b> pushed every 10 seconds</div>"
        "<div class='mc-row'><b>Upstream</b> upsd with dummy-ups driver</div>"
        "<div class='mc-row'><b>Requires</b> esppush user (actions=SET)</div>"
        "<div class='mc-foot'>"
          "NUT clients query the upstream upsd - not this device.<br><br>"
          "Upstream needs a pre-declared ups.dev file so dummy-ups "
          "can accept all pushed variables.<br><br>"
          "See docs/nut-upstream-setup.md"
        "</div>"
        "</div>"

        /* Card 3 - Bridge */
        "<div id='mc2' class='mode-card' style='display:none'>"
        "<div class='mc-tag'>Mode 3</div>"
        "<div class='mc-name'>Raw HID Bridge</div>"
        "<div class='mc-row'><b>Forwards</b> raw USB HID bytes over TCP</div>"
        "<div class='mc-row'><b>No decoding</b> performed on device</div>"
        "<div class='mc-row'><b>Sends</b> full HID descriptor on connect</div>"
        "<div class='mc-row'><b>Streams</b> all interrupt-IN packets live</div>"
        "<div class='mc-row'><b>Upstream</b> handles all decode and NUT serving</div>"
        "<div class='mc-foot'>"
          "Wire format: [2B desc length][descriptor bytes] handshake, "
          "then [1B type][2B length][data] per packet.<br><br>"
          "type 0x01 = interrupt-IN data<br>"
          "type 0xFF = keepalive (no data, 5s idle)"
        "</div>"
        "</div>"

        "</div>"  /* end mode-info */
        "</div>"  /* end page-wrap */

        "<script>"
        "function onModeChange(v){"
          "document.getElementById('upstream_sec').style.display=(v=='2'||v=='3')?'':'none';"
          "document.getElementById('upstream_err').style.display='none';"
          "['mc0','mc1','mc2'].forEach(function(id,i){"
            "document.getElementById(id).style.display=(v==String(i+1))?'':'none';"
          "});"
        "}"
        "function chkSave(){"
          "var v=document.querySelector('[name=op_mode]').value;"
          "if(v=='2'||v=='3'){"
            "var h=document.getElementById('upstream_host').value.trim();"
            "if(!h){"
              "document.getElementById('upstream_err').style.display='';"
              "document.getElementById('upstream_host').focus();"
              "return false;"
            "}"
          "}"
          "return true;"
        "}"
        "onModeChange(document.querySelector('[name=op_mode]').value);"
        "</script>"
        "</body></html>",

        pw_warn, note_html,
        sel_sa, sel_nc, sel_br,
        sel_hid, sel_qs,
        cfg->sta_ssid, cfg->sta_pass,
        ap_up ? "Active" : "Off",
        cfg->ap_ssid, cfg->ap_pass,
        cfg->ups_name, cfg->nut_user, cfg->nut_pass,
        up_disp, cfg->upstream_host, (unsigned)up_port,
        sta_ip[0] ? sta_ip : "not connected"
    );
}

void parse_form_kv(app_cfg_t *cfg_inout, const char *body,
                   char *action_out, size_t action_sz)
{
    if (action_out && action_sz) action_out[0] = 0;

    char tmp[HTTP_BODY_MAX + 1];
    strlcpy0(tmp, body ? body : "", sizeof(tmp));

    char *saveptr = NULL;
    for (char *tok = strtok_r(tmp, "&", &saveptr); tok; tok = strtok_r(NULL, "&", &saveptr)) {
        char *eq = strchr(tok, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = tok, *v = eq + 1;
        url_decode_inplace(k);
        url_decode_inplace(v);
        trim_ws(k);
        if      (!strcmp(k, "sta_ssid"))   strlcpy0(cfg_inout->sta_ssid,   v, sizeof(cfg_inout->sta_ssid));
        else if (!strcmp(k, "sta_pass"))   strlcpy0(cfg_inout->sta_pass,   v, sizeof(cfg_inout->sta_pass));
        else if (!strcmp(k, "ap_ssid"))    strlcpy0(cfg_inout->ap_ssid,    v, sizeof(cfg_inout->ap_ssid));
        else if (!strcmp(k, "ap_pass"))    strlcpy0(cfg_inout->ap_pass,    v, sizeof(cfg_inout->ap_pass));
        else if (!strcmp(k, "ups_name"))   strlcpy0(cfg_inout->ups_name,   v, sizeof(cfg_inout->ups_name));
        else if (!strcmp(k, "nut_user"))   strlcpy0(cfg_inout->nut_user,   v, sizeof(cfg_inout->nut_user));
        else if (!strcmp(k, "nut_pass"))   strlcpy0(cfg_inout->nut_pass,   v, sizeof(cfg_inout->nut_pass));
        else if (!strcmp(k, "portal_pass") && v[0])
            strlcpy0(cfg_inout->portal_pass, v, sizeof(cfg_inout->portal_pass));
        else if (!strcmp(k, "op_mode")) {
            int m = atoi(v);
            cfg_inout->op_mode = (m >= 1 && m <= 3) ? (uint8_t)m : OP_MODE_STANDALONE;
        }
        else if (!strcmp(k, "ups_protocol")) {
            int p = atoi(v);
            cfg_inout->ups_protocol = (p == UPS_PROTO_QS) ? UPS_PROTO_QS : UPS_PROTO_HID;
        }
        else if (!strcmp(k, "upstream_host"))
            strlcpy0(cfg_inout->upstream_host, v, sizeof(cfg_inout->upstream_host));
        else if (!strcmp(k, "upstream_port")) {
            int p = atoi(v);
            cfg_inout->upstream_port = (p > 0 && p <= 65535) ? (uint16_t)p : 3493;
        }
        else if (!strcmp(k, "action") && action_out && action_sz)
            strlcpy0(action_out, v, action_sz);
    }
}
