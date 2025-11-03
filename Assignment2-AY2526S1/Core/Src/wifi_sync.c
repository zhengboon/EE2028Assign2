#include "wifi_sync.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32l4xx_hal.h"

extern void printu(const char *fmt, ...);

#define WIFI_SYNC_SOCKET_RX      0U
#define WIFI_SYNC_SOCKET_TX      1U
#define WIFI_SYNC_MAX_MESSAGE    64U
#define WIFI_SYNC_MAX_PEERS      8U

typedef struct {
    uint8_t  in_use;
    uint32_t id;
    uint8_t  ip[4];
    uint32_t last_seen_ms;
    uint8_t  announced;
} wifi_peer_info_t;

typedef struct {
    wifi_sync_config_t cfg;
    uint8_t  ready;
    uint8_t  connected;
    uint8_t  sockets_open;
    uint8_t  peer_count;
    wifi_peer_info_t peers[WIFI_SYNC_MAX_PEERS];
    uint32_t board_id;
    uint32_t last_poll_ms;
    uint32_t poll_interval_ms;
    uint32_t rx_timeout_ms;
    uint32_t next_connect_ms;
    uint32_t reconnect_interval_ms;
    uint32_t last_hello_ms;
    uint32_t hello_interval_ms;
    uint32_t device_check_interval_ms;
    uint32_t last_peer_scan_ms;
    char     last_error[80];
    wifi_sync_event_t pending_event;
} wifi_sync_state_t;

static wifi_sync_state_t g_sync = {0U};

static void wifi_sync_set_error(const char *msg);
static void wifi_sync_reset_peer_list(void);
static uint8_t wifi_sync_record_peer(uint32_t remote_id, const uint8_t ip[4], uint32_t now_ms);
static void wifi_sync_print_device_count(void);
static void wifi_sync_close_sockets(void);
static uint8_t wifi_sync_start_transport(void);
static void wifi_sync_handle_disconnect(const char *reason);
static uint8_t wifi_sync_send_common(uint8_t game, wifi_sync_event_type_t type);
static uint8_t wifi_sync_send_hello(void);
static void wifi_sync_handle_message(const char *msg, const uint8_t ipaddr[4], uint32_t now_ms);
static uint8_t wifi_sync_attempt_connect(uint32_t now_ms);
static void wifi_sync_apply_config(const wifi_sync_config_t *cfg, wifi_sync_config_t *out);
static void wifi_sync_peer_scan(uint32_t now_ms);
static void wifi_sync_format_ip(char *buf, size_t buf_len, const uint8_t ip[4]);

static void wifi_sync_set_error(const char *msg)
{
    if (msg == NULL) {
        g_sync.last_error[0] = '\0';
        return;
    }
    strncpy(g_sync.last_error, msg, sizeof(g_sync.last_error) - 1U);
    g_sync.last_error[sizeof(g_sync.last_error) - 1U] = '\0';
}

static void wifi_sync_reset_peer_list(void)
{
    g_sync.peer_count = 0U;
    memset(g_sync.peers, 0, sizeof(g_sync.peers));
}

static uint8_t wifi_sync_record_peer(uint32_t remote_id, const uint8_t ip[4], uint32_t now_ms)
{
    if ((remote_id == 0U) || (remote_id == g_sync.board_id)) {
        return 0U;
    }

    wifi_peer_info_t *slot = NULL;
    for (uint8_t i = 0U; i < WIFI_SYNC_MAX_PEERS; ++i) {
        if (g_sync.peers[i].in_use && g_sync.peers[i].id == remote_id) {
            slot = &g_sync.peers[i];
            break;
        }
    }

    uint8_t is_new = 0U;
    if (slot == NULL) {
        for (uint8_t i = 0U; i < WIFI_SYNC_MAX_PEERS; ++i) {
            if (!g_sync.peers[i].in_use) {
                slot = &g_sync.peers[i];
                memset(slot, 0, sizeof(*slot));
                slot->in_use = 1U;
                slot->id = remote_id;
                slot->announced = 0U;
                g_sync.peer_count++;
                is_new = 1U;
                break;
            }
        }
    }

    if (slot == NULL) {
        return 0U;
    }

    if (ip != NULL) {
        memcpy(slot->ip, ip, sizeof(slot->ip));
    } else {
        memset(slot->ip, 0, sizeof(slot->ip));
    }
    slot->last_seen_ms = now_ms;
    return is_new;
}

static void wifi_sync_print_device_count(void)
{
    if (!g_sync.connected) {
        return;
    }
    uint8_t active_peers = 0U;
    for (uint8_t i = 0U; i < WIFI_SYNC_MAX_PEERS; ++i) {
        if (g_sync.peers[i].in_use && g_sync.peers[i].announced) {
            active_peers++;
        }
    }
    uint8_t total = (uint8_t)(1U + active_peers);
    printu("WiFi sync: %u device(s) detected (including self)\r\n", total);
}

static void wifi_sync_close_sockets(void)
{
    if (!g_sync.sockets_open) {
        return;
    }

    (void)WIFI_CloseClientConnection(WIFI_SYNC_SOCKET_TX);
    (void)WIFI_StopServer(WIFI_SYNC_SOCKET_RX);
    g_sync.sockets_open = 0U;
}

static uint8_t wifi_sync_start_transport(void)
{
    WIFI_Status_t status = WIFI_StartServer(WIFI_SYNC_SOCKET_RX,
                                            WIFI_UDP_PROTOCOL,
                                            "sync_rx",
                                            g_sync.cfg.port);
    if (status != WIFI_STATUS_OK) {
        return 0U;
    }

    status = WIFI_OpenClientConnection(WIFI_SYNC_SOCKET_TX,
                                       WIFI_UDP_PROTOCOL,
                                       "sync_tx",
                                       g_sync.cfg.peer_ip,
                                       g_sync.cfg.port,
                                       0U);
    if (status != WIFI_STATUS_OK) {
        (void)WIFI_StopServer(WIFI_SYNC_SOCKET_RX);
        return 0U;
    }

    g_sync.sockets_open = 1U;
    return 1U;
}

static void wifi_sync_handle_disconnect(const char *reason)
{
    if (!g_sync.ready) {
        return;
    }

    if (g_sync.connected) {
        if (reason != NULL) {
            printu("WiFi sync: connection lost (%s)\r\n", reason);
        } else {
            printu("WiFi sync: connection lost\r\n");
        }
    }

    wifi_sync_close_sockets();
    (void)WIFI_Disconnect();
    g_sync.connected = 0U;
    g_sync.next_connect_ms = HAL_GetTick() + g_sync.reconnect_interval_ms;
    wifi_sync_reset_peer_list();
    g_sync.last_peer_scan_ms = HAL_GetTick();
}

static uint8_t wifi_sync_send_common(uint8_t game, wifi_sync_event_type_t type)
{
    if (!g_sync.ready || !g_sync.connected || !g_sync.sockets_open) {
        return 0U;
    }

    char message[WIFI_SYNC_MAX_MESSAGE];
    int written = snprintf(message,
                           sizeof(message),
                           "EE28,%u,%u,%08lX\n",
                           (unsigned int)type,
                           (unsigned int)game,
                           (unsigned long)g_sync.board_id);
    if (written <= 0) {
        return 0U;
    }

    uint16_t sent_len = 0U;
    WIFI_Status_t status = WIFI_SendDataTo(WIFI_SYNC_SOCKET_TX,
                                           (uint8_t *)message,
                                           (uint16_t)written,
                                           &sent_len,
                                           2000U,
                                           g_sync.cfg.peer_ip,
                                           g_sync.cfg.port);
    if (status != WIFI_STATUS_OK) {
        wifi_sync_handle_disconnect("send failed");
        return 0U;
    }
    return 1U;
}

static uint8_t wifi_sync_send_hello(void)
{
    return wifi_sync_send_common(0U, WIFI_SYNC_EVENT_HELLO);
}

static void wifi_sync_handle_message(const char *msg, const uint8_t ipaddr[4], uint32_t now_ms)
{
    if ((msg == NULL) || (strncmp(msg, "EE28,", 5) != 0)) {
        return;
    }

    const char *cursor = msg + 5;
    long evt = strtol(cursor, (char **)&cursor, 10);
    if (*cursor != ',') {
        return;
    }
    cursor++;

    long game = strtol(cursor, (char **)&cursor, 10);
    if (*cursor != ',') {
        return;
    }
    cursor++;

    unsigned long remote_id = strtoul(cursor, NULL, 16);
    uint8_t is_new_peer = wifi_sync_record_peer((uint32_t)remote_id, ipaddr, now_ms);
    if (is_new_peer) {
        (void)wifi_sync_send_hello();
    }

    if (remote_id == g_sync.board_id) {
        return;
    }

    if (g_sync.pending_event.type == WIFI_SYNC_EVENT_NONE) {
        g_sync.pending_event.type = (wifi_sync_event_type_t)evt;
        g_sync.pending_event.game = (uint8_t)game;
        g_sync.pending_event.remote_id = (uint32_t)remote_id;
    }
}

static void wifi_sync_format_ip(char *buf, size_t buf_len, const uint8_t ip[4])
{
    if ((buf == NULL) || (buf_len == 0U)) {
        return;
    }
    snprintf(buf, buf_len, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

static void wifi_sync_peer_scan(uint32_t now_ms)
{
    uint8_t change = 0U;
    for (uint8_t i = 0U; i < WIFI_SYNC_MAX_PEERS; ++i) {
        wifi_peer_info_t *peer = &g_sync.peers[i];
        if (!peer->in_use) {
            continue;
        }

        uint32_t elapsed = now_ms - peer->last_seen_ms;
        uint8_t seen_recent = (elapsed <= g_sync.device_check_interval_ms);
        if (seen_recent) {
            if (!peer->announced) {
                char ipbuf[20];
                wifi_sync_format_ip(ipbuf, sizeof(ipbuf), peer->ip);
                printu("New device connected, IP:%s\r\n", ipbuf);
                peer->announced = 1U;
                change = 1U;
            }
        } else {
            char ipbuf[20];
            wifi_sync_format_ip(ipbuf, sizeof(ipbuf), peer->ip);
            printu("Device IP:%s disconnected\r\n", ipbuf);
            peer->announced = 0U;
            peer->in_use = 0U;
            if (g_sync.peer_count > 0U) {
                g_sync.peer_count--;
            }
            memset(peer, 0, sizeof(*peer));
            change = 1U;
        }
    }

    if (change) {
        wifi_sync_print_device_count();
    }
}

static void wifi_sync_apply_config(const wifi_sync_config_t *cfg, wifi_sync_config_t *out)
{
    if (cfg != NULL) {
        *out = *cfg;
    } else {
        out->ssid = WIFI_SYNC_DEFAULT_SSID;
        out->password = WIFI_SYNC_DEFAULT_PASSWORD;
        out->security = WIFI_SYNC_DEFAULT_SECURITY;
        out->peer_ip[0] = WIFI_SYNC_DEFAULT_PEER_IP0;
        out->peer_ip[1] = WIFI_SYNC_DEFAULT_PEER_IP1;
        out->peer_ip[2] = WIFI_SYNC_DEFAULT_PEER_IP2;
        out->peer_ip[3] = WIFI_SYNC_DEFAULT_PEER_IP3;
        out->port = WIFI_SYNC_DEFAULT_PORT;
        out->poll_interval_ms = WIFI_SYNC_DEFAULT_POLL_MS;
        out->rx_timeout_ms = WIFI_SYNC_DEFAULT_RX_TIMEOUT;
    }

    if (out->ssid == NULL) {
        out->ssid = WIFI_SYNC_DEFAULT_SSID;
    }
    if (out->password == NULL) {
        out->password = WIFI_SYNC_DEFAULT_PASSWORD;
    }
    if (out->security > WIFI_ECN_WPA_WPA2_PSK) {
        out->security = WIFI_SYNC_DEFAULT_SECURITY;
    }
    if ((out->peer_ip[0] | out->peer_ip[1] | out->peer_ip[2] | out->peer_ip[3]) == 0U) {
        out->peer_ip[0] = WIFI_SYNC_DEFAULT_PEER_IP0;
        out->peer_ip[1] = WIFI_SYNC_DEFAULT_PEER_IP1;
        out->peer_ip[2] = WIFI_SYNC_DEFAULT_PEER_IP2;
        out->peer_ip[3] = WIFI_SYNC_DEFAULT_PEER_IP3;
    }
    if (out->port == 0U) {
        out->port = WIFI_SYNC_DEFAULT_PORT;
    }
    if (out->poll_interval_ms == 0U) {
        out->poll_interval_ms = WIFI_SYNC_DEFAULT_POLL_MS;
    }
    if (out->rx_timeout_ms == 0U) {
        out->rx_timeout_ms = WIFI_SYNC_DEFAULT_RX_TIMEOUT;
    }
}

static uint8_t wifi_sync_attempt_connect(uint32_t now_ms)
{
    WIFI_Status_t status = WIFI_Connect(g_sync.cfg.ssid,
                                        g_sync.cfg.password,
                                        g_sync.cfg.security);
    if (status != WIFI_STATUS_OK) {
        wifi_sync_set_error("WIFI_Connect failed");
        printu("WiFi sync: unable to connect to %s\r\n", g_sync.cfg.ssid);
        g_sync.next_connect_ms = now_ms + g_sync.reconnect_interval_ms;
        return 0U;
    }

    if (!wifi_sync_start_transport()) {
        wifi_sync_set_error("socket setup failed");
        printu("WiFi sync: socket setup failed\r\n");
        (void)WIFI_Disconnect();
        g_sync.next_connect_ms = now_ms + g_sync.reconnect_interval_ms;
        return 0U;
    }

    g_sync.connected = 1U;
    g_sync.next_connect_ms = now_ms + g_sync.reconnect_interval_ms;
    g_sync.last_poll_ms = now_ms;
    g_sync.last_hello_ms = 0U;
    g_sync.last_peer_scan_ms = now_ms;
    wifi_sync_reset_peer_list();
    wifi_sync_set_error(NULL);

    printu("WiFi sync: connected to %s\r\n", g_sync.cfg.ssid);

    uint8_t ipaddr[4] = {0U};
    if (WIFI_GetIP_Address(ipaddr) == WIFI_STATUS_OK) {
        printu("WiFi sync: board IP %u.%u.%u.%u\r\n",
               ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);
    }

    wifi_sync_print_device_count();

    if (!wifi_sync_send_hello()) {
        return 0U;
    }
    g_sync.last_hello_ms = HAL_GetTick();
    wifi_sync_peer_scan(now_ms);
    return 1U;
}

uint8_t WifiSync_Init(const wifi_sync_config_t *config)
{
    wifi_sync_config_t cfg;
    wifi_sync_apply_config(config, &cfg);

    memset(&g_sync, 0, sizeof(g_sync));
    g_sync.cfg = cfg;
    g_sync.poll_interval_ms = cfg.poll_interval_ms;
    g_sync.rx_timeout_ms = cfg.rx_timeout_ms;
    g_sync.reconnect_interval_ms = WIFI_SYNC_RECONNECT_INTERVAL_MS;
    g_sync.hello_interval_ms = WIFI_SYNC_HELLO_INTERVAL_MS;
    g_sync.device_check_interval_ms = WIFI_SYNC_RECONNECT_INTERVAL_MS;
    g_sync.last_peer_scan_ms = 0U;
    g_sync.pending_event.type = WIFI_SYNC_EVENT_NONE;
    wifi_sync_set_error(NULL);
    g_sync.board_id = HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2();

    if (WIFI_Init() != WIFI_STATUS_OK) {
        wifi_sync_set_error("WIFI_Init failed");
        printu("WiFi sync: WIFI_Init failed\r\n");
        return 0U;
    }

    g_sync.ready = 1U;
    g_sync.next_connect_ms = HAL_GetTick();
    (void)wifi_sync_attempt_connect(g_sync.next_connect_ms);
    return 1U;
}

uint8_t WifiSync_IsReady(void)
{
    return g_sync.ready;
}

const char *WifiSync_GetLastError(void)
{
    return g_sync.last_error[0] ? g_sync.last_error : NULL;
}

uint8_t WifiSync_IsConnected(void)
{
    return (g_sync.ready && g_sync.connected);
}

uint8_t WifiSync_GetDeviceCount(void)
{
    if (!WifiSync_IsConnected()) {
        return 0U;
    }
    uint8_t count = 1U;
    for (uint8_t i = 0U; i < WIFI_SYNC_MAX_PEERS; ++i) {
        if (g_sync.peers[i].in_use && g_sync.peers[i].announced) {
            count++;
        }
    }
    return count;
}

void WifiSync_Process(uint32_t now_ms)
{
    if (!g_sync.ready) {
        return;
    }

    if (!g_sync.connected) {
        if ((int32_t)(now_ms - g_sync.next_connect_ms) >= 0) {
            (void)wifi_sync_attempt_connect(now_ms);
        }
        if (!g_sync.connected) {
            return;
        }
    }

    if ((int32_t)(now_ms - g_sync.last_poll_ms) >= (int32_t)g_sync.poll_interval_ms) {
        g_sync.last_poll_ms = now_ms;

        uint8_t rx_buf[WIFI_SYNC_MAX_MESSAGE];
        uint16_t rx_len = 0U;
        uint8_t ipaddr[4] = {0U};
        uint16_t port = 0U;

        WIFI_Status_t status = WIFI_ReceiveDataFrom(WIFI_SYNC_SOCKET_RX,
                                                    rx_buf,
                                                    (uint16_t)(sizeof(rx_buf) - 1U),
                                                    &rx_len,
                                                    g_sync.rx_timeout_ms,
                                                    ipaddr,
                                                    &port);
        if ((status == WIFI_STATUS_OK) && (rx_len > 0U)) {
            rx_buf[rx_len] = '\0';
            wifi_sync_handle_message((const char *)rx_buf, ipaddr, now_ms);
        }
    }

    if ((int32_t)(now_ms - g_sync.last_peer_scan_ms) >= (int32_t)g_sync.device_check_interval_ms) {
        g_sync.last_peer_scan_ms = now_ms;
        wifi_sync_peer_scan(now_ms);
    }

    if ((int32_t)(now_ms - g_sync.last_hello_ms) >= (int32_t)g_sync.hello_interval_ms) {
        if (wifi_sync_send_hello()) {
            g_sync.last_hello_ms = now_ms;
        }
    }
}

uint8_t WifiSync_PopEvent(wifi_sync_event_t *event)
{
    if ((event == NULL) || (g_sync.pending_event.type == WIFI_SYNC_EVENT_NONE)) {
        return 0U;
    }

    *event = g_sync.pending_event;
    g_sync.pending_event.type = WIFI_SYNC_EVENT_NONE;
    g_sync.pending_event.game = 0U;
    g_sync.pending_event.remote_id = 0U;
    return 1U;
}

uint8_t WifiSync_SendGameOver(uint8_t game)
{
    return wifi_sync_send_common(game, WIFI_SYNC_EVENT_GAME_OVER);
}

uint8_t WifiSync_SendReset(uint8_t game)
{
    return wifi_sync_send_common(game, WIFI_SYNC_EVENT_RESET);
}
