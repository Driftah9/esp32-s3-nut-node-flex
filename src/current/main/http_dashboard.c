/*============================================================================
 MODULE: http_dashboard

 RESPONSIBILITY
 - Renders GET / -- UPS status dashboard with AJAX live polling
 - Full NUT variable table directly on page (upsc-style)
 - AJAX polls /status every 5s and updates all fields live

 REVERT HISTORY
 R0  v15.11  Split from http_portal.c
 R1  v15.11  Fix AJAX addOrUpdate ID mismatch; rid=0x21 runtime; Smart-UPS C cache
 R2  v15.14  NUT Variables lightbox (click link)
 R3  v0.6-flex  Full upsc-style NUT variable table directly on dashboard.
 R4  v0.12-flex Diagnostic log capture section: radio (90s/120s) + Start Capture
                button. Shows progress banner when armed, View Log link when ready.
                All groups (battery/input/output/ups/device/driver) visible.
                Live AJAX updates every 5s. Lightbox removed.
                Mode shown in subtitle. ups.vendorid/productid added.
 R5  v0.17  Replace hardcoded "v0.6-flex" subtitle version string with
            esp_app_get_description()->version so it tracks the built firmware.
 R6  v0.21  Adaptive AJAX poll rate: 1500ms while ups_valid=false (waiting for
            first data), 5000ms once valid. Prevents the long visible delay
            between data becoming ready on the ESP and the page updating.
            data_age shows "Waiting for UPS data..." while not yet valid.
============================================================================*/

#include "http_dashboard.h"
#include "http_portal_css.h"
#include "ups_state.h"
#include "wifi_mgr.h"
#include "cfg_store.h"
#include "diag_capture.h"
#include "esp_app_desc.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

void render_dashboard(app_cfg_t *cfg, char *out, size_t outsz)
{
    ups_state_t ups;
    ups_state_snapshot(&ups);

    const char *st = ups.ups_status[0] ? ups.ups_status : "UNKNOWN";
    char sta_ip[16];
    wifi_mgr_sta_ip_str(sta_ip);

    const char *st_cls = "status-unknown";
    if      (strstr(st, "OB") || strstr(st, "CHRG"))                   st_cls = "status-ob";
    else if (strstr(st, "OL"))                                          st_cls = "status-ol";
    else if (strstr(st, "LB") || strstr(st, "RB") || strstr(st, "ALARM")) st_cls = "status-fault";

    const char *pw_warn = cfg_store_is_default_pass(cfg)
        ? "<div class='warn'>Default password in use. "
          "<a href='/config'>Change it in Config.</a></div>"
        : "";

    const char *fw_ver = esp_app_get_description()->version;

    /* Subtitle reflects active mode */
    const char *mode_sub;
    switch (cfg->op_mode) {
        case OP_MODE_NUT_CLIENT: mode_sub = "Mode 2 &mdash; NUT Client push"; break;
        case OP_MODE_BRIDGE:     mode_sub = "Mode 3 &mdash; Raw HID Bridge";  break;
        default:                 mode_sub = "Mode 1 &mdash; Standalone NUT server on tcp/3493"; break;
    }

    /* Build diagnostic capture section - state-dependent */
    char diag_sec[768];
    if (diag_capture_is_armed()) {
        uint32_t el  = diag_capture_get_elapsed_s();
        uint32_t dur = diag_capture_get_duration();
        uint32_t rem = (el < dur) ? (dur - el) : 0;
        snprintf(diag_sec, sizeof(diag_sec),
            "<div class='warn' style='margin-top:16px'>"
            "Log capture in progress: %us elapsed, ~%us remaining."
            " <a href='/diag-log'>Check status</a></div>",
            (unsigned)el, (unsigned)rem);
    } else if (diag_capture_is_ready()) {
        snprintf(diag_sec, sizeof(diag_sec),
            "<div style='margin-top:16px;font-family:Arial,sans-serif;"
            "font-size:0.9em'>"
            "<span style='color:#4fc3f7'>Log capture complete.</span>"
            " <a href='/diag-log' target='_blank'>View Log</a>"
            "</div>");
    } else {
        snprintf(diag_sec, sizeof(diag_sec),
            "<form method='POST' action='/diag-start' style='margin-top:20px'>"
            "<div class='form-section'>Diagnostic Log Capture</div>"
            "<div class='form-row'><span class='form-label'>Duration</span>"
            "<label style='margin-right:16px'>"
            "<input type='radio' name='dur' value='90' checked> 90s</label>"
            "<label style='margin-right:16px'>"
            "<input type='radio' name='dur' value='120'> 120s</label>"
            "<label><input type='radio' name='dur' value='300'> 300s</label>"
            "</div>"
            "<div style='margin-top:10px'>"
            "<input class='btn' type='submit' value='Start Capture'>"
            "</div>"
            "<div style='color:#555;font-size:0.78em;margin-top:8px;"
            "font-family:Arial,sans-serif'>"
            "Reboots device and captures full boot log. WiFi credentials, network names, upstream host, and passwords are redacted before display."
            "</div></form>");
    }

    snprintf(out, outsz,
        "<!doctype html><html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>ESP32 UPS Node</title>"
        PORTAL_CSS

        /* NUT table overrides */
        "<style>"
        ".st{display:inline-block;font-size:1.25em;font-weight:bold;"
            "letter-spacing:0.06em;margin-bottom:18px;font-family:Arial,sans-serif}"
        ".nt{margin-bottom:16px}"
        ".nt td{padding:3px 10px;border:1px solid #1c1c1c;"
               "font-family:'Courier New',Courier,monospace;font-size:0.87em}"
        ".nt .nk{color:#666;width:52%%}"
        ".nt .nv{color:#d0d0d0}"
        ".nt .ol{color:#00c853;font-weight:bold}"
        ".nt .ob{color:#ffab00;font-weight:bold}"
        ".nt .na{color:#3a3a3a}"
        ".nt .ng td{color:#4fc3f7;font-size:0.71em;text-transform:uppercase;"
                   "letter-spacing:0.10em;background:#0d0d0d;"
                   "padding:5px 10px;border-top:2px solid #222;border-bottom:none}"
        "</style>"
        "</head><body>"

        "<h2>ESP32 UPS Node</h2>"
        "<div class='subtitle'>%s &mdash; %s</div>"
        "%s"

        "<div class='st %s'>%s</div>"
        "<div id='data_age' style='font-size:0.72em;color:#555;"
             "font-family:Arial,sans-serif;margin-bottom:12px'></div>"

        "<table class='nt'>"

        /* device - first, identifies what is plugged in */
        "<tr class='ng'><td colspan='2'>device</td></tr>"
        "<tr><td class='nk'>device.mfr</td>"
            "<td class='nv' id='v_dmfr'>...</td></tr>"
        "<tr><td class='nk'>device.model</td>"
            "<td class='nv' id='v_dmdl'>...</td></tr>"
        "<tr><td class='nk'>device.serial</td>"
            "<td class='nv' id='v_dser'>...</td></tr>"
        "<tr><td class='nk'>device.type</td>"
            "<td class='nv'>ups</td></tr>"

        /* driver */
        "<tr class='ng'><td colspan='2'>driver</td></tr>"
        "<tr><td class='nk'>driver.name</td>"
            "<td class='nv'>esp32-nut-hid</td></tr>"
        "<tr><td class='nk'>driver.version</td>"
            "<td class='nv' id='v_dver'>...</td></tr>"

        /* battery */
        "<tr class='ng'><td colspan='2'>battery</td></tr>"
        "<tr><td class='nk'>battery.charge</td>"
            "<td class='nv' id='v_bc'>...</td></tr>"
        "<tr><td class='nk'>battery.charge.low</td>"
            "<td class='nv' id='v_bcl'>...</td></tr>"
        "<tr><td class='nk'>battery.charge.warning</td>"
            "<td class='nv' id='v_bcw'>...</td></tr>"
        "<tr><td class='nk'>battery.runtime</td>"
            "<td class='nv' id='v_brt'>...</td></tr>"
        "<tr><td class='nk'>battery.runtime.low</td>"
            "<td class='nv' id='v_brl'>...</td></tr>"
        "<tr><td class='nk'>battery.type</td>"
            "<td class='nv'>PbAc</td></tr>"
        "<tr><td class='nk'>battery.voltage</td>"
            "<td class='nv' id='v_bv'>...</td></tr>"
        "<tr><td class='nk'>battery.voltage.nominal</td>"
            "<td class='nv' id='v_bvn'>...</td></tr>"

        /* input */
        "<tr class='ng'><td colspan='2'>input</td></tr>"
        "<tr><td class='nk'>input.utility.present</td>"
            "<td class='nv' id='v_iup'>...</td></tr>"
        "<tr><td class='nk'>input.voltage</td>"
            "<td class='nv' id='v_iv'>...</td></tr>"
        "<tr><td class='nk'>input.voltage.nominal</td>"
            "<td class='nv' id='v_ivn'>...</td></tr>"

        /* output */
        "<tr class='ng'><td colspan='2'>output</td></tr>"
        "<tr><td class='nk'>output.voltage</td>"
            "<td class='nv' id='v_ov'>...</td></tr>"

        /* ups */
        "<tr class='ng'><td colspan='2'>ups</td></tr>"
        "<tr><td class='nk'>ups.delay.shutdown</td>"
            "<td class='nv'>20 s</td></tr>"
        "<tr><td class='nk'>ups.delay.start</td>"
            "<td class='nv'>30 s</td></tr>"
        "<tr><td class='nk'>ups.firmware</td>"
            "<td class='nv' id='v_ufw'>...</td></tr>"
        "<tr><td class='nk'>ups.load</td>"
            "<td class='nv' id='v_ul'>...</td></tr>"
        "<tr><td class='nk'>ups.mfr</td>"
            "<td class='nv' id='v_umfr'>...</td></tr>"
        "<tr><td class='nk'>ups.model</td>"
            "<td class='nv' id='v_umdl'>...</td></tr>"
        "<tr><td class='nk'>ups.productid</td>"
            "<td class='nv' id='v_upid'>...</td></tr>"
        "<tr><td class='nk'>ups.status</td>"
            "<td class='nv' id='v_ust'>%s</td></tr>"
        "<tr><td class='nk'>ups.test.result</td>"
            "<td class='nv'>None</td></tr>"
        "<tr><td class='nk'>ups.timer.reboot</td>"
            "<td class='nv'>-1</td></tr>"
        "<tr><td class='nk'>ups.timer.shutdown</td>"
            "<td class='nv'>-1</td></tr>"
        "<tr><td class='nk'>ups.type</td>"
            "<td class='nv' id='v_utp'>...</td></tr>"
        "<tr><td class='nk'>ups.vendorid</td>"
            "<td class='nv' id='v_uvid'>...</td></tr>"

        /* system */
        "<tr class='ng'><td colspan='2'>system</td></tr>"
        "<tr><td class='nk'>system.uptime</td>"
            "<td class='nv' id='v_uptime'>...</td></tr>"

        "</table>"

        "<div class='poll' id='td_poll'></div>"
        "<div class='nav'>"
        "<a href='/config'>Configure</a>"
        "<a href='/status'>Status JSON</a>"
        "<a href='/compat'>Compatible UPS List</a>"
        "<a href='/reboot' onclick=\"return confirm('Reboot device?')\">Reboot</a>"
        "<span style='color:#555;font-size:0.82em;margin-left:8px' id='td_ip'>%s</span>"
        "</div>"

        "%s"  /* diag capture section */

        "<script>"
        /* helpers */
        "function sv(id,val,cls){"
          "var e=document.getElementById(id);"
          "if(!e)return;"
          "e.textContent=val;"
          "e.className='nv'+(cls?' '+cls:'');"
        "}"
        "function na(v,sfx){"
          "return(v!==null&&v!==undefined&&v!=='unknown')?v+(sfx||''):'n/a';"
        "}"
        "function naRaw(v,sfx){"    /* na but keeps 'unknown' values */
          "return(v!==null&&v!==undefined)?v+(sfx||''):'n/a';"
        "}"
        "function fmtRt(s){"
          "if(s===null||s===undefined||s==='null')return 'n/a';"
          "var h=Math.floor(s/3600);"
          "var m=Math.floor((s%%3600)/60);"
          "var sc=s%%60;"
          "var r='';"
          "if(h>0)r+=h+'h ';"
          "if(m>0||h>0)r+=('0'+m).slice(-2)+'m ';"
          "r+=('0'+sc).slice(-2)+'s ('+s+'s)';"
          "return r;"
        "}"
        "function stCls(s){"
          "if(s.indexOf('OB')>=0||s.indexOf('DISCHRG')>=0)return 'ob';"
          "if(s.indexOf('OL')>=0||s.indexOf('CHRG')>=0)return 'ol';"
          "return '';"
        "}"
        "function fmtTime(d){"
          "var h=d.getHours(),m=d.getMinutes(),s=d.getSeconds();"
          "var ap=h>=12?'PM':'AM';h=h%%12||12;"
          "return h+':'+(m<10?'0':'')+m+':'+(s<10?'0':'')+s+' '+ap;"
        "}"
        /* Adaptive poll rate: 1500ms while ups_valid=false, 5000ms once valid */
        "var lastOk=null;"
        "var pollTimer=null;"
        "function schedPoll(ms){"
          "if(pollTimer)clearTimeout(pollTimer);"
          "pollTimer=setTimeout(doPoll,ms);"
        "}"
        "setInterval(function(){"
          "var el=document.getElementById('td_poll');"
          "var now=fmtTime(new Date());"
          "el.textContent=lastOk?('Now: '+now+' | Last poll: '+fmtTime(lastOk)):('Now: '+now+' | Polling...');"
        "},1000);"

        /* poll */
        "function doPoll(){"
          "var x=new XMLHttpRequest();"
          "x.open('GET','/status',true);"
          "x.onload=function(){"
            "if(x.status!==200){schedPoll(1500);return;}"
            "try{"
              "var d=JSON.parse(x.responseText);"
              "var sc=d.ups_status||'UNKNOWN';"
              "var scl=stCls(sc);"
              /* status badge */
              "var sb=document.querySelector('.st');"
              "if(sb){"
                "sb.textContent=sc;"
                "sb.className='st status-'+(scl==='ob'?'ob':scl==='ol'?'ol':'unknown');"
              "}"
              /* battery */
              "sv('v_bc', d.battery_charge!==undefined?d.battery_charge+'%%':'n/a');"
              "sv('v_bcl',d.battery_charge_low!==undefined?d.battery_charge_low+'%%':'n/a');"
              "sv('v_bcw',d.battery_charge_warning!==undefined?d.battery_charge_warning+'%%':'n/a');"
              "sv('v_brt',fmtRt(d.battery_runtime_s));"
              "sv('v_brl',d.battery_runtime_low_s!==null?fmtRt(d.battery_runtime_low_s):'n/a',d.battery_runtime_low_s===null?'na':'');"
              "sv('v_bv', d.battery_voltage_v!==null?d.battery_voltage_v.toFixed(3)+' V':'n/a',d.battery_voltage_v===null?'na':'');"
              "sv('v_bvn',d.battery_voltage_nominal_v!==null?d.battery_voltage_nominal_v+' V':'n/a',d.battery_voltage_nominal_v===null?'na':'');"
              /* input */
              "sv('v_iup',sc.indexOf('OL')>=0?'1':'0');"
              "sv('v_iv', d.input_voltage_v!==null?d.input_voltage_v.toFixed(1)+' V':'n/a',d.input_voltage_v===null?'na':'');"
              "sv('v_ivn',d.input_voltage_nominal_v!==null?d.input_voltage_nominal_v+' V':'n/a',d.input_voltage_nominal_v===null?'na':'');"
              /* output */
              "sv('v_ov', d.output_voltage_v!==null?d.output_voltage_v.toFixed(1)+' V':'n/a',d.output_voltage_v===null?'na':'');"
              /* ups */
              "sv('v_ufw', naRaw(d.ups_firmware));"
              "sv('v_ul',  d.ups_load_pct!==null?d.ups_load_pct+'%%':'n/a',d.ups_load_pct===null?'na':'');"
              "sv('v_umfr',naRaw(d.device_mfr));"
              "sv('v_umdl',naRaw(d.device_model));"
              "sv('v_upid',na(d.ups_productid));"
              "sv('v_ust', sc,scl);"
              /* Data age: waiting message while not yet valid, age once valid */
              "var agEl=document.getElementById('data_age');"
              "if(agEl){"
                "if(!d.ups_valid){"
                  "agEl.textContent='Waiting for UPS data...';"
                "}else if(d.data_age_ms!==undefined){"
                  "var ag=d.data_age_ms;"
                  "var ags=ag<2000?(ag+'ms'):(Math.round(ag/1000)+'s');"
                  "agEl.textContent='UPS data: '+ags+' old';"
                "}"
              "}"
              "sv('v_utp', naRaw(d.ups_type));"
              "sv('v_uvid',na(d.ups_vendorid));"
              /* uptime */
              "if(d.uptime_s!=null){"
                "var ut=d.uptime_s,ud=Math.floor(ut/86400),uh=Math.floor((ut%86400)/3600),"
                "um=Math.floor((ut%3600)/60),us=ut%60;"
                "var uts='';"
                "if(ud>0)uts+=ud+'d ';"
                "uts+=('0'+uh).slice(-2)+':'+('0'+um).slice(-2)+':'+('0'+us).slice(-2);"
                "sv('v_uptime',uts);"
              "}else{sv('v_uptime','n/a');}"
              /* device */
              "sv('v_dmfr',naRaw(d.device_mfr));"
              "sv('v_dmdl',naRaw(d.device_model));"
              "sv('v_dser',naRaw(d.device_serial));"
              /* driver */
              "sv('v_dver',naRaw(d.driver_version));"
              /* nav ip */
              "document.getElementById('td_ip').textContent=d.sta_ip||'';"
              "lastOk=new Date();"
              /* schedule next poll: fast while waiting for data, slow once live */
              "schedPoll(d.ups_valid?5000:1500);"
            "}catch(e){schedPoll(1500);}"
          "};"
          "x.onerror=function(){schedPoll(1500);};"
          "x.send();"
        "}"
        "doPoll();"
        "</script>"
        "</body></html>",

        fw_ver,             /* firmware version in subtitle */
        mode_sub,           /* subtitle mode string */
        pw_warn,            /* password warning */
        st_cls, st,         /* status badge: class + text */
        st,                 /* ups.status initial value in table */
        sta_ip[0] ? sta_ip : "",
        diag_sec            /* diagnostic capture section */
    );
}
