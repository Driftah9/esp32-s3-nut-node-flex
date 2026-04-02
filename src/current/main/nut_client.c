/*============================================================================
 MODULE: nut_client
 VERSION: v0.1-flex
 DATE: 2026-04-02

 RESPONSIBILITY
 - Mode 2: connect to upstream upsd as a NUT client
 - Authenticate with USERNAME / PASSWORD
 - Push live UPS state via SET VAR on each poll cycle
 - Variables pushed: battery.charge, battery.runtime, ups.status,
   battery.voltage, ups.load, ups.flags, input.utility.present
 - Reconnects automatically on disconnect
 - Falls back to nut_server_start() if upstream unreachable at boot
   and cfg->upstream_fallback == 1

 NUT PUSH PROTOCOL (standard NUT client commands)
   USERNAME <user>\n
   PASSWORD <pass>\n
   SET VAR <ups_name> <variable> "<value>"\n
   -> upsd replies OK or ERR ...

 UPSTREAM SETUP (dummy-ups on LXC)
   User: esppush / esppush123
   Device: esp-ups
   Server: 10.0.0.18:3493

============================================================================*/

#include "nut_client.h"
#include "nut_server.h"
#include "ups_state.h"
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

/* --------------------------------------------------------------------------
 * Helpers
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

    /* Wait for connect with select */
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

    /* Check SO_ERROR to confirm connect succeeded */
    int err = 0;
    socklen_t elen = sizeof(err);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &elen);
    if (err != 0) {
        ESP_LOGW(TAG, "connect(%s:%u) SO_ERROR=%d", host, port, err);
        close(fd);
        return -1;
    }

    /* Restore blocking mode with recv timeout */
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    struct timeval rtv = { .tv_sec = NUT_CLIENT_RECV_TIMEOUT_S, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rtv, sizeof(rtv));

    ESP_LOGI(TAG, "connected to %s:%u", host, port);
    return fd;
}

/* Send a line and read the response. Returns true if response starts with "OK" */
static bool nc_cmd(int fd, char *resp_buf, size_t resp_sz, const char *fmt, ...)
{
    char cmd[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);

    /* Append newline if missing */
    size_t len = strlen(cmd);
    if (len > 0 && cmd[len - 1] != '\n') {
        if (len + 1 < sizeof(cmd)) { cmd[len] = '\n'; cmd[len + 1] = 0; len++; }
    }

    if (send(fd, cmd, (int)len, 0) < 0) {
        ESP_LOGW(TAG, "send failed: errno=%d", errno);
        return false;
    }

    /* Read response line */
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

    /* Strip trailing CR */
    len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n')) buf[--len] = 0;

    if (resp_buf && resp_sz > 0) {
        strlcpy(resp_buf, buf, resp_sz);
    }

    bool ok = (strncmp(buf, "OK", 2) == 0);
    if (!ok) {
        ESP_LOGW(TAG, "upstream ERR on cmd '%.*s': %s", (int)(strlen(cmd) - 1), cmd, buf);
    }
    return ok;
}

/* Push one SET VAR. Returns false if the send/recv fails (connection dead). */
static bool nc_set_var(int fd, const char *ups_name, const char *var, const char *val_fmt, ...)
{
    char val[64];
    va_list ap;
    va_start(ap, val_fmt);
    vsnprintf(val, sizeof(val), val_fmt, ap);
    va_end(ap);

    char resp[64];
    bool ok = nc_cmd(fd, resp, sizeof(resp), "SET VAR %s %s \"%s\"\n", ups_name, var, val);
    if (!ok) {
        /* Distinguish NUT-level reject (ERR) from socket failure */
        if (resp[0] != 0) {
            ESP_LOGW(TAG, "SET VAR %s %s rejected: %s", ups_name, var, resp);
            return true;   /* upsd responded - connection is alive */
        }
        ESP_LOGW(TAG, "SET VAR %s %s - no response (connection dead)", ups_name, var);
        return false;      /* socket dead */
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
 * Push one full UPS state snapshot.
 * Returns false if a mandatory push fails (connection dead) - triggers reconnect.
 * -------------------------------------------------------------------------- */

static bool nc_push_state(int fd, const char *ups_name)
{
    ups_state_t st;
    ups_state_snapshot(&st);

    /* Mandatory: failure here means connection is dead */
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

    /* Optional: only push when valid - failures here are non-fatal */
    if (st.battery_runtime_valid)
        nc_set_var(fd, ups_name, "battery.runtime", "%u", (unsigned)st.battery_runtime_s);
    if (st.battery_voltage_valid)
        nc_set_var(fd, ups_name, "battery.voltage", "%.3f",
                   (double)st.battery_voltage_mv / 1000.0);
    if (st.ups_load_valid)
        nc_set_var(fd, ups_name, "ups.load", "%u", (unsigned)st.ups_load_pct);

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
    const char *ups_name = cfg->ups_name[0] ? cfg->ups_name : "esp-ups";

    ESP_LOGI(TAG, "Mode 2 NUT CLIENT - target %s:%u ups=%s", host, port, ups_name);

    /* Wait for DHCP and ARP table to settle before first connect attempt.
     * WiFi STA connect fires ~1.8s after boot; DHCP typically adds ~1-2s more.
     * 5s covers both without meaningfully delaying normal operation. */
    ESP_LOGI(TAG, "waiting 5s for network ready...");
    vTaskDelay(pdMS_TO_TICKS(5000));

    /* Boot-time reachability check */
    if (!host[0]) {
        ESP_LOGE(TAG, "upstream_host not configured");
        if (cfg->upstream_fallback) {
            ESP_LOGW(TAG, "upstream_host empty - falling back to STANDALONE");
            nut_server_start(cfg);
        }
        vTaskDelete(NULL);
        return;
    }

    /* Initial connect attempt to confirm reachability before committing to Mode 2 */
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
            ESP_LOGE(TAG, "auth failed - reconnecting in %ds", NUT_CLIENT_RECONNECT_DELAY_MS / 1000);
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
