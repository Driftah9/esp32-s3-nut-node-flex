/*============================================================================
 MODULE: nut_bridge
 VERSION: v0.1-flex
 DATE: 2026-04-02

 RESPONSIBILITY
 - Mode 3: raw HID bridge — forward USB interrupt-IN stream to upstream host
 - USB HID interrupt-IN packets are queued by the USB task callback
 - Bridge task dequeues and sends over TCP
 - HID Report Descriptor sent as handshake on connect
 - Upstream (LXC receiver) handles all decoding and NUT serving

 WIRE PROTOCOL
   Handshake (once per connection):
     [2 bytes BE: desc_len] [desc_len bytes: raw HID Report Descriptor]

   Per interrupt-IN packet:
     [1 byte: type=0x01] [2 bytes BE: data_len] [data_len bytes: raw data]

   Keepalive (if no packet for BRIDGE_KEEPALIVE_MS):
     [1 byte: type=0xFF] [2 bytes BE: 0]

 UPSTREAM
   Receiver script: /opt/nut-bridge/bridge_receiver.py on nut-test-lxc
   Listens on upstream_port (e.g. 5493)
   Saves descriptor to /tmp/hid-desc.bin, logs packet stream

============================================================================*/

#include "nut_bridge.h"
#include "nut_server.h"
#include "ups_usb_hid.h"
#include "cfg_store.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

static const char *TAG = "nut_bridge";

/* Packet type bytes */
#define PKT_TYPE_INTR_IN   0x01u
#define PKT_TYPE_KEEPALIVE 0xFFu

/* Queue depth: number of interrupt-IN packets bufferable between USB task and bridge task */
#define BRIDGE_QUEUE_DEPTH 32

/* Max interrupt-IN MPS. HID UPS devices use 8-64 bytes typically. */
#define BRIDGE_PKT_MAX  64

/* Keepalive interval — send a keepalive packet if no intr-IN arrives within this time */
#define BRIDGE_KEEPALIVE_MS  5000

/* Connect timeout (seconds) */
#define BRIDGE_CONNECT_TIMEOUT_S  5

/* Reconnect delay after disconnect */
#define BRIDGE_RECONNECT_DELAY_MS 15000

/* Startup delay to allow DHCP and USB enumeration */
#define BRIDGE_STARTUP_DELAY_MS   6000

/* How long to wait for the HID descriptor before giving up */
#define BRIDGE_DESC_WAIT_MS       20000
#define BRIDGE_DESC_POLL_MS       500

typedef struct {
    uint8_t  data[BRIDGE_PKT_MAX];
    uint16_t len;
} bridge_pkt_t;

static QueueHandle_t s_queue = NULL;

/* --------------------------------------------------------------------------
 * Interrupt-IN callback (called from USB client task)
 * -------------------------------------------------------------------------- */

static void bridge_intr_cb(const uint8_t *data, uint16_t len)
{
    if (!s_queue || len == 0 || len > BRIDGE_PKT_MAX) return;
    bridge_pkt_t pkt;
    memcpy(pkt.data, data, len);
    pkt.len = len;
    /* Non-blocking: drop packet if queue full (prefer USB task responsiveness) */
    xQueueSend(s_queue, &pkt, 0);
}

/* --------------------------------------------------------------------------
 * TCP connect (non-blocking with select timeout)
 * -------------------------------------------------------------------------- */

static int bridge_connect(const char *host, uint16_t port)
{
    struct addrinfo hints = {0};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);

    struct addrinfo *res = NULL;
    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) {
        ESP_LOGW(TAG, "getaddrinfo(%s) failed", host);
        return -1;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int rc = connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (rc != 0 && errno != EINPROGRESS) {
        ESP_LOGW(TAG, "connect(%s:%u) failed: errno=%d", host, port, errno);
        close(fd);
        return -1;
    }

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);
    struct timeval tv = { .tv_sec = BRIDGE_CONNECT_TIMEOUT_S, .tv_usec = 0 };
    rc = select(fd + 1, NULL, &wfds, NULL, &tv);
    if (rc <= 0) {
        ESP_LOGW(TAG, "connect(%s:%u) timed out", host, port);
        close(fd);
        return -1;
    }

    int err = 0; socklen_t elen = sizeof(err);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &elen);
    if (err != 0) {
        ESP_LOGW(TAG, "connect(%s:%u) SO_ERROR=%d", host, port, err);
        close(fd);
        return -1;
    }

    /* Restore blocking */
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    struct timeval stv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &stv, sizeof(stv));

    ESP_LOGI(TAG, "connected to %s:%u", host, port);
    return fd;
}

/* --------------------------------------------------------------------------
 * Send all bytes — returns false on partial/failed write
 * -------------------------------------------------------------------------- */

static bool bridge_send_all(int fd, const uint8_t *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        int r = send(fd, data + sent, (int)(len - sent), 0);
        if (r <= 0) return false;
        sent += (size_t)r;
    }
    return true;
}

/* --------------------------------------------------------------------------
 * Send handshake: [2B BE: desc_len][desc bytes]
 * Waits up to BRIDGE_DESC_WAIT_MS for USB enumeration to complete
 * -------------------------------------------------------------------------- */

static bool bridge_send_handshake(int fd)
{
    const uint8_t *desc = NULL;
    uint16_t desc_len   = 0;

    int waited = 0;
    while (waited < BRIDGE_DESC_WAIT_MS) {
        if (ups_usb_hid_get_report_descriptor(&desc, &desc_len)) break;
        vTaskDelay(pdMS_TO_TICKS(BRIDGE_DESC_POLL_MS));
        waited += BRIDGE_DESC_POLL_MS;
    }

    if (!desc || desc_len == 0) {
        ESP_LOGE(TAG, "HID Report Descriptor not available after %dms - no UPS connected?", waited);
        return false;
    }

    ESP_LOGI(TAG, "sending handshake: descriptor %u bytes", (unsigned)desc_len);

    uint8_t hdr[2] = {
        (uint8_t)(desc_len >> 8),
        (uint8_t)(desc_len & 0xFF)
    };
    if (!bridge_send_all(fd, hdr, 2)) {
        ESP_LOGW(TAG, "handshake: failed to send descriptor length header");
        return false;
    }
    if (!bridge_send_all(fd, desc, desc_len)) {
        ESP_LOGW(TAG, "handshake: failed to send descriptor payload");
        return false;
    }

    ESP_LOGI(TAG, "handshake sent OK - streaming interrupt-IN packets");
    return true;
}

/* --------------------------------------------------------------------------
 * Send one packet: [1B: type][2B BE: data_len][data bytes]
 * -------------------------------------------------------------------------- */

static bool bridge_send_packet(int fd, uint8_t type, const uint8_t *data, uint16_t data_len)
{
    uint8_t hdr[3] = {
        type,
        (uint8_t)(data_len >> 8),
        (uint8_t)(data_len & 0xFF)
    };
    if (!bridge_send_all(fd, hdr, 3)) return false;
    if (data_len > 0 && data) {
        if (!bridge_send_all(fd, data, data_len)) return false;
    }
    return true;
}

/* --------------------------------------------------------------------------
 * Main bridge task
 * -------------------------------------------------------------------------- */

static void nut_bridge_task(void *arg)
{
    app_cfg_t *cfg = (app_cfg_t *)arg;

    const char *host = cfg->upstream_host;
    uint16_t    port = cfg->upstream_port ? cfg->upstream_port : 5493;

    ESP_LOGI(TAG, "Mode 3 BRIDGE - target %s:%u", host, port);

    if (!host[0]) {
        ESP_LOGE(TAG, "upstream_host not configured");
        if (cfg->upstream_fallback) nut_server_start(cfg);
        vTaskDelete(NULL);
        return;
    }

    /* Register interrupt-IN callback */
    ups_usb_hid_set_bridge_cb(bridge_intr_cb);

    /* Wait for DHCP and USB enumeration */
    ESP_LOGI(TAG, "waiting %ds for network + USB ready...", BRIDGE_STARTUP_DELAY_MS / 1000);
    vTaskDelay(pdMS_TO_TICKS(BRIDGE_STARTUP_DELAY_MS));

    /* Boot reachability check */
    int fd = bridge_connect(host, port);
    if (fd < 0) {
        ESP_LOGW(TAG, "upstream %s:%u unreachable at boot", host, port);
        ups_usb_hid_set_bridge_cb(NULL);
        if (cfg->upstream_fallback) {
            ESP_LOGW(TAG, "falling back to STANDALONE (nut_server)");
            nut_server_start(cfg);
        } else {
            ESP_LOGE(TAG, "upstream unreachable and fallback disabled");
        }
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        /* Send handshake descriptor */
        if (!bridge_send_handshake(fd)) {
            ESP_LOGE(TAG, "handshake failed - reconnecting");
            close(fd);
            fd = -1;
            goto reconnect;
        }

        /* Packet stream loop */
        TickType_t keepalive_ticks = pdMS_TO_TICKS(BRIDGE_KEEPALIVE_MS);
        bridge_pkt_t pkt;

        while (1) {
            BaseType_t got = xQueueReceive(s_queue, &pkt, keepalive_ticks);
            if (got == pdTRUE) {
                if (!bridge_send_packet(fd, PKT_TYPE_INTR_IN, pkt.data, pkt.len)) {
                    ESP_LOGW(TAG, "send failed (connection dead) - reconnecting");
                    break;
                }
                ESP_LOGD(TAG, "fwd %u bytes", (unsigned)pkt.len);
            } else {
                /* Keepalive */
                if (!bridge_send_packet(fd, PKT_TYPE_KEEPALIVE, NULL, 0)) {
                    ESP_LOGW(TAG, "keepalive send failed - reconnecting");
                    break;
                }
                ESP_LOGD(TAG, "keepalive sent");
            }
        }

        close(fd);
        fd = -1;

reconnect:
        ESP_LOGI(TAG, "reconnecting in %ds...", BRIDGE_RECONNECT_DELAY_MS / 1000);
        /* Drain stale packets from queue before reconnect */
        while (xQueueReceive(s_queue, &pkt, 0) == pdTRUE) {}
        vTaskDelay(pdMS_TO_TICKS(BRIDGE_RECONNECT_DELAY_MS));

        fd = bridge_connect(host, port);
        if (fd < 0) {
            ESP_LOGW(TAG, "reconnect failed - will retry");
            goto reconnect;
        }
    }
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void nut_bridge_start(app_cfg_t *cfg)
{
    s_queue = xQueueCreate(BRIDGE_QUEUE_DEPTH, sizeof(bridge_pkt_t));
    if (!s_queue) {
        ESP_LOGE(TAG, "failed to create packet queue");
        if (cfg->upstream_fallback) nut_server_start(cfg);
        return;
    }
    xTaskCreate(nut_bridge_task, "nut_bridge", 8192, cfg, 5, NULL);
}
