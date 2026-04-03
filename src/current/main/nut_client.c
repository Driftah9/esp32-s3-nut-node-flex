/*============================================================================
 MODULE: nut_client
 VERSION: v0.2-flex
 DATE: 2026-04-02

 RESPONSIBILITY
 - Mode 2: connect to upstream upsd as a NUT client
 - Authenticate with USERNAME / PASSWORD
 - Push full UPS identity + static variables once on connect
 - Push live dynamic UPS state on each poll cycle
 - Reconnects automatically on disconnect
 - Falls back to nut_server_start() if upstream unreachable at boot
   and cfg->upstream_fallback == 1

 VARIABLES PUSHED

 Identity push (nc_push_identity - once per connection, after auth):
   device.mfr         - manufacturer name from USB strings
   device.model       - product name from USB strings (DB-overridden)
   device.serial      - USB serial string (or UNKNOWN)
   device.type        - static: "ups"
   ups.mfr            - same as device.mfr
   ups.model          - same as device.model
   ups.firmware       - firmware version string (if non-empty)
   ups.vendorid       - USB VID as 4-char hex
   ups.productid      - USB PID as 4-char hex
   battery.type       - static: "PbAc"
   battery.charge.low - low-battery threshold from device DB (default 20%)
   battery.charge.warning   - warning threshold from DB (if non-zero)
   battery.runtime.low      - low-runtime threshold from DB in seconds (if non-zero)
   battery.voltage.nominal  - nominal battery voltage from DB in V (if non-zero)
   input.voltage.nominal    - nominal input voltage from DB in V (if non-zero)
   ups.type                 - from DB (default "line-interactive")
   ups.test.result    - static: "No test initiated"
   ups.delay.shutdown - static: "20"
   ups.delay.start    - static: "30"
   ups.timer.reboot   - static: "-1"
   ups.timer.shutdown - static: "-1"

 State push (nc_push_state - every NUT_CLIENT_PUSH_INTERVAL_MS):
   battery.charge     - current charge %
   ups.status         - NUT status string (OL / OB / OB DISCHRG / OL LB etc.)
   input.utility.present - 1 if AC present, 0 if on battery
   ups.flags          - raw ESP flags bitmask
   battery.runtime    - estimated runtime in seconds (when valid)
   battery.voltage    - current battery voltage in V (when valid)
   ups.load           - load percent (when valid)
   input.voltage      - AC input voltage in V (when valid, GET_REPORT devices)
   output.voltage     - AC output voltage in V (when valid, GET_REPORT devices)

 NUT PUSH PROTOCOL (standard NUT client commands)
   USERNAME <user>
   PASSWORD <pass>
   SET VAR <ups_name> <variable> "<value>"
   -> upsd replies OK or ERR ...

 UPSTREAM SETUP
   Server: upsd with dummy-ups driver
   User:   esppush (actions=SET in upsd.users)
   Device: ups.dev must pre-declare all variables (dummy-ups cannot create
           new variables via SET VAR - they must exist in the .dev file first)
   See: docs/nut-upstream-setup.md for full upstream configuration guide

============================================================================*/

#include "nut_client.h"
#include "nut_server.h"
#include "ups_state.h"
#include "ups_device_db.h"
#include "cfg_store.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

static const char *TAG = "nut_client";

/* Push interval when connected */
#define NUT_CLIENT_PUSH_INTERVAL_MS   10000

/* Reconnect delay after disconnect or initial failure */
#define NUT_CLIENT_RECONNECT_DELAY_MS 15000

/* Connect timeout (seconds) */
#define NUT_CLIENT_CONNECT_TIMEOUT_S  5

/* Receive timeout on socket */
#define NUT_CLIENT_RECV_TIMEOUT_S     5

/* NUT user and password the ESP authenticates with on the upstream */
#define NUT_PUSH_USER  "esppush"
#define NUT_PUSH_PASS  "esppush123"

/* Static UPS properties pushed on every connection */
#define BATTERY_TYPE   "PbAc"
#define DEVICE_TYPE    "ups"
#define UPS_TYPE_DEFAULT "line-interactive"

/* Helper: return str if non-empty, otherwise fallback */
static const char *str_or(const char *s, const char *fallback)
{
    return (s && s[0]) ? s : fallback;
}

/* --------------------------------------------------------------------------
 * TCP connect with non-blocking timeout
 * -------------------------------------------------------------------------- */

static int nc_connect(const char *host, uint16_t port)
{
    struct addrinfo hints = {0};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);

    struct addrinfo *res = NULL;
    int rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0 || !res) {
        ESP_LOGW(TAG, "getaddrinfo(%s) failed: %d", host, rc);
        return -1;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        ESP_LOGE(TAG, "socket() failed: errno=%d", errno);
        freeaddrinfo(res);
        return -1;
    }

    /* Non-blocking connect with select timeout */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    rc = connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (rc != 0 && errno != EINPROGRESS) {
        ESP_LOGW(TAG, "connect(%s:%u) failed immediately: errno=%d", host, port, errno);
        close(fd);
        return -1;
    }

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);
    struct timeval tv = { .tv_sec = NUT_CLIENT_CONNECT_TIMEOUT_S, .tv_usec = 0 };

    rc = select(fd + 1, NULL, &wfds, NULL, &tv);
    if (rc <= 0) {
        ESP_LOGW(TAG, "connect(%s:%u) timed out", host, port);
        close(fd);
        return -1;
    }

    int err = 0;
    socklen_t elen = sizeof(err);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &elen);
    if (err != 0) {
        ESP_LOGW(TAG, "connect(%s:%u) SO_ERROR=%d", host, port, err);
        close(fd);
        return -1;
    }

    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    struct timeval rtv = { .tv_sec = NUT_CLIENT_RECV_TIMEOUT_S, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rtv, sizeof(rtv));

    ESP_LOGI(TAG, "connected to %s:%u", host, port);
    return fd;
}

/* --------------------------------------------------------------------------
 * Send one command, read one response line.
 * Returns true if response starts with "OK".
 * -------------------------------------------------------------------------- */

static bool nc_cmd(int fd, char *resp_buf, size_t resp_sz, const char *fmt, ...)
{
    char cmd[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);

    size_t len = strlen(cmd);
    if (len > 0 && cmd[len - 1] != '\n') {
        if (len + 1 < sizeof(cmd)) { cmd[len] = '\n'; cmd[len + 1] = 0; len++; }
    }

    if (send(fd, cmd, (int)len, 0) < 0) {
        ESP_LOGW(TAG, "send failed: errno=%d", errno);
        return false;
    }

    char buf[256] = {0};
    int used = 0;
    while (used < (int)sizeof(buf) - 1) {
        int r = recv(fd, buf + used, 1, 0);
        if (r <= 0) {
            ESP_LOGW(TAG, "recv failed: errno=%d r=%d", errno, r);
            return false;
        }
        if (buf[used] == '\n') { buf[used] = 0; break; }
        used++;
    }

    len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n')) buf[--len] = 0;

    if (resp_buf && resp_sz > 0)
        strlcpy(resp_buf, buf, resp_sz);

    bool ok = (strncmp(buf, "OK", 2) == 0);
    if (!ok)
        ESP_LOGW(TAG, "upstream ERR on cmd '%.*s': %s", (int)(strlen(cmd) - 1), cmd, buf);
    return ok;
}

/* --------------------------------------------------------------------------
 * Push one SET VAR.
 * Returns false only if the socket is dead (no response at all).
 * NUT-level rejection (ERR UNKNOWN-UPS, ERR READONLY, etc.) returns true
 * because the socket is still alive - the ESP just logs the rejection.
 * -------------------------------------------------------------------------- */

static bool nc_set_var(int fd, const char *ups_name, const char *var,
                       const char *val_fmt, ...)
{
    char val[128];
    va_list ap;
    va_start(ap, val_fmt);
    vsnprintf(val, sizeof(val), val_fmt, ap);
    va_end(ap);

    char resp[64];
    bool ok = nc_cmd(fd, resp, sizeof(resp),
                     "SET VAR %s %s \"%s\"\n", ups_name, var, val);
    if (!ok) {
        if (resp[0] != 0) {
            /* upsd responded with ERR - variable rejected but socket alive */
            ESP_LOGW(TAG, "SET VAR %s %s rejected: %s", ups_name, var, resp);
            return true;
        }
        /* No response at all - socket dead */
        ESP_LOGW(TAG, "SET VAR %s %s - no response (connection dead)", ups_name, var);
        return false;
    }
    ESP_LOGD(TAG, "SET VAR %s %s = %s", ups_name, var, val);
    return true;
}

/* --------------------------------------------------------------------------
 * Auth sequence
 * -------------------------------------------------------------------------- */

static bool nc_auth(int fd)
{
    char resp[64];
    if (!nc_cmd(fd, resp, sizeof(resp), "USERNAME %s\n", NUT_PUSH_USER)) {
        ESP_LOGE(TAG, "USERNAME rejected: %s", resp);
        return false;
    }
    if (!nc_cmd(fd, resp, sizeof(resp), "PASSWORD %s\n", NUT_PUSH_PASS)) {
        ESP_LOGE(TAG, "PASSWORD rejected: %s", resp);
        return false;
    }
    ESP_LOGI(TAG, "authenticated as %s", NUT_PUSH_USER);
    return true;
}

/* --------------------------------------------------------------------------
 * Identity push - called once per connection after auth.
 * Pushes all static/identity variables that do not change between polls.
 * These are the variables Mode 1 STANDALONE also serves.
 * Returns false if socket dies mid-push.
 * -------------------------------------------------------------------------- */

static bool nc_push_identity(int fd, const char *ups_name)
{
    ups_state_t st;
    ups_state_snapshot(&st);

    /* Look up device DB entry for nominal/static values */
    const ups_device_entry_t *db = ups_device_db_lookup(st.vid, st.pid);

    /* --- Device identity from USB enumeration --- */
    if (!nc_set_var(fd, ups_name, "device.mfr",    "%s", str_or(st.manufacturer, "UNKNOWN")))
        return false;
    if (!nc_set_var(fd, ups_name, "device.model",  "%s", str_or(st.product,      "UNKNOWN")))
        return false;
    if (!nc_set_var(fd, ups_name, "device.serial", "%s", str_or(st.serial,       "UNKNOWN")))
        return false;
    if (!nc_set_var(fd, ups_name, "device.type",   "ups"))
        return false;
    if (!nc_set_var(fd, ups_name, "ups.mfr",       "%s", str_or(st.manufacturer, "UNKNOWN")))
        return false;
    if (!nc_set_var(fd, ups_name, "ups.model",     "%s", str_or(st.product,      "UNKNOWN")))
        return false;

    /* Optional identity fields */
    if (st.ups_firmware[0])
        nc_set_var(fd, ups_name, "ups.firmware",   "%s", st.ups_firmware);
    if (st.vid)
        nc_set_var(fd, ups_name, "ups.vendorid",   "%04x", (unsigned)st.vid);
    if (st.pid)
        nc_set_var(fd, ups_name, "ups.productid",  "%04x", (unsigned)st.pid);

    /* --- Static hardware properties --- */
    nc_set_var(fd, ups_name, "battery.type",      BATTERY_TYPE);
    nc_set_var(fd, ups_name, "battery.charge.low", "%u", (unsigned)st.battery_charge_low);

    /* --- DB-sourced nominal/threshold values --- */
    if (db) {
        const char *ups_type = db->ups_type ? db->ups_type : UPS_TYPE_DEFAULT;
        nc_set_var(fd, ups_name, "ups.type", "%s", ups_type);

        if (db->battery_charge_warning)
            nc_set_var(fd, ups_name, "battery.charge.warning",
                       "%u", (unsigned)db->battery_charge_warning);

        if (db->battery_runtime_low_s)
            nc_set_var(fd, ups_name, "battery.runtime.low",
                       "%u", (unsigned)db->battery_runtime_low_s);

        if (db->battery_voltage_nominal_mv)
            nc_set_var(fd, ups_name, "battery.voltage.nominal",
                       "%.1f", (double)db->battery_voltage_nominal_mv / 1000.0);

        if (db->input_voltage_nominal_v)
            nc_set_var(fd, ups_name, "input.voltage.nominal",
                       "%u", (unsigned)db->input_voltage_nominal_v);
    } else {
        nc_set_var(fd, ups_name, "ups.type", UPS_TYPE_DEFAULT);
    }

    /* --- Static NUT protocol housekeeping variables --- */
    nc_set_var(fd, ups_name, "ups.test.result",    "None");
    nc_set_var(fd, ups_name, "ups.delay.shutdown", "20");
    nc_set_var(fd, ups_name, "ups.delay.start",    "30");
    nc_set_var(fd, ups_name, "ups.timer.reboot",   "-1");
    nc_set_var(fd, ups_name, "ups.timer.shutdown", "-1");

    ESP_LOGI(TAG, "identity pushed: mfr=%s model=%s vid=%04x pid=%04x",
             str_or(st.manufacturer, "UNKNOWN"),
             str_or(st.product,      "UNKNOWN"),
             (unsigned)st.vid,
             (unsigned)st.pid);
    return true;
}

/* --------------------------------------------------------------------------
 * State push - called every NUT_CLIENT_PUSH_INTERVAL_MS.
 * Pushes dynamic variables that change with UPS condition.
 * Returns false if socket dies mid-push (mandatory variable send failure).
 * -------------------------------------------------------------------------- */

static bool nc_push_state(int fd, const char *ups_name)
{
    ups_state_t st;
    ups_state_snapshot(&st);

    /* Mandatory: failure here means connection is dead - triggers reconnect */
    if (!nc_set_var(fd, ups_name, "battery.charge", "%u", (unsigned)st.battery_charge))
        return false;
    if (!nc_set_var(fd, ups_name, "ups.status", "%s",
                    st.ups_status[0] ? st.ups_status : "OL"))
        return false;
    if (!nc_set_var(fd, ups_name, "input.utility.present", "%u",
                    st.input_utility_present ? 1u : 0u))
        return false;
    if (!nc_set_var(fd, ups_name, "ups.flags", "0x%08X", (unsigned)st.ups_flags))
        return false;

    /* Optional: only push when valid - non-fatal if variable not in .dev file */
    if (st.battery_runtime_valid)
        nc_set_var(fd, ups_name, "battery.runtime",
                   "%u", (unsigned)st.battery_runtime_s);
    if (st.battery_voltage_valid)
        nc_set_var(fd, ups_name, "battery.voltage",
                   "%.3f", (double)st.battery_voltage_mv / 1000.0);
    if (st.ups_load_valid)
        nc_set_var(fd, ups_name, "ups.load",
                   "%u", (unsigned)st.ups_load_pct);
    if (st.input_voltage_valid)
        nc_set_var(fd, ups_name, "input.voltage",
                   "%.1f", (double)st.input_voltage_mv / 1000.0);
    if (st.output_voltage_valid)
        nc_set_var(fd, ups_name, "output.voltage",
                   "%.1f", (double)st.output_voltage_mv / 1000.0);

    return true;
}

/* --------------------------------------------------------------------------
 * Main push task
 * -------------------------------------------------------------------------- */

static void nut_client_task(void *arg)
{
    app_cfg_t *cfg = (app_cfg_t *)arg;

    const char *host     = cfg->upstream_host;
    uint16_t    port     = cfg->upstream_port ? cfg->upstream_port : 3493;
    const char *ups_name = cfg->ups_name[0] ? cfg->ups_name : "ups";

    ESP_LOGI(TAG, "Mode 2 NUT CLIENT - target %s:%u ups=%s", host, port, ups_name);

    /* Wait for DHCP and ARP table to settle before first connect attempt.
     * WiFi STA connect fires ~1.8s after boot; DHCP typically adds ~1-2s more.
     * 5s covers both without meaningfully delaying normal operation. */
    ESP_LOGI(TAG, "waiting 5s for network ready...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    if (!host[0]) {
        ESP_LOGE(TAG, "upstream_host not configured");
        if (cfg->upstream_fallback) {
            ESP_LOGW(TAG, "upstream_host empty - falling back to STANDALONE");
            nut_server_start(cfg);
        }
        vTaskDelete(NULL);
        return;
    }

    /* Boot-time reachability check */
    int fd = nc_connect(host, port);
    if (fd < 0) {
        ESP_LOGW(TAG, "upstream %s:%u unreachable at boot", host, port);
        if (cfg->upstream_fallback) {
            ESP_LOGW(TAG, "falling back to STANDALONE (nut_server)");
            nut_server_start(cfg);
        } else {
            ESP_LOGE(TAG, "upstream unreachable and fallback disabled - no NUT service");
        }
        vTaskDelete(NULL);
        return;
    }

    /* Authenticated session loop */
    while (1) {
        if (!nc_auth(fd)) {
            ESP_LOGE(TAG, "auth failed - reconnecting in %ds",
                     NUT_CLIENT_RECONNECT_DELAY_MS / 1000);
            close(fd);
            fd = -1;
            goto reconnect;
        }

        /* Push identity once on connect - device info, nominals, static vars */
        if (!nc_push_identity(fd, ups_name)) {
            ESP_LOGW(TAG, "identity push failed - reconnecting");
            close(fd);
            fd = -1;
            goto reconnect;
        }

        ESP_LOGI(TAG, "push loop started - interval %dms", NUT_CLIENT_PUSH_INTERVAL_MS);

        while (1) {
            if (!nc_push_state(fd, ups_name)) {
                ESP_LOGW(TAG, "push failed (connection dead) - reconnecting");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(NUT_CLIENT_PUSH_INTERVAL_MS));
        }

        close(fd);
        fd = -1;

reconnect:
        ESP_LOGI(TAG, "reconnecting in %ds...", NUT_CLIENT_RECONNECT_DELAY_MS / 1000);
        vTaskDelay(pdMS_TO_TICKS(NUT_CLIENT_RECONNECT_DELAY_MS));

        fd = nc_connect(host, port);
        if (fd < 0) {
            ESP_LOGW(TAG, "reconnect failed - will retry");
            goto reconnect;
        }
    }
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void nut_client_start(app_cfg_t *cfg)
{
    xTaskCreate(nut_client_task, "nut_client", 8192, cfg, 5, NULL);
}
