#ifndef WIFI_SYNC_H
#define WIFI_SYNC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* User configuration                                                         */
/* -------------------------------------------------------------------------- */

/* Update these macros with your hotspot credentials before building */
#define WIFI_SYNC_DEFAULT_SSID         "zhengboon"
#define WIFI_SYNC_DEFAULT_PASSWORD     "665615189104"
#define WIFI_SYNC_DEFAULT_SECURITY     WIFI_ECN_WPA2_PSK

/* Remote peer address (set to broadcast by default: 255.255.255.255) */
#define WIFI_SYNC_DEFAULT_PEER_IP0     255U
#define WIFI_SYNC_DEFAULT_PEER_IP1     255U
#define WIFI_SYNC_DEFAULT_PEER_IP2     255U
#define WIFI_SYNC_DEFAULT_PEER_IP3     255U

/* UDP port both boards will use for sync messages */
#define WIFI_SYNC_DEFAULT_PORT         50505U

/* How often to poll the Wi-Fi module for packets (in milliseconds) */
#define WIFI_SYNC_DEFAULT_POLL_MS      100U

/* Timeout passed to WIFI_ReceiveDataFrom (in milliseconds) */
#define WIFI_SYNC_DEFAULT_RX_TIMEOUT   50U

/* Interval between automatic reconnection attempts (milliseconds) */
#define WIFI_SYNC_RECONNECT_INTERVAL_MS 30000U

/* Interval between presence beacons while connected (milliseconds) */
#define WIFI_SYNC_HELLO_INTERVAL_MS     10000U

/* -------------------------------------------------------------------------- */

typedef enum {
    WIFI_SYNC_EVENT_NONE = 0,
    WIFI_SYNC_EVENT_GAME_OVER = 1,
    WIFI_SYNC_EVENT_RESET = 2,
    WIFI_SYNC_EVENT_HELLO = 3
} wifi_sync_event_type_t;

typedef struct {
    wifi_sync_event_type_t type;
    uint8_t game;
    uint32_t remote_id;
} wifi_sync_event_t;

typedef struct {
    const char *ssid;
    const char *password;
    WIFI_Ecn_t security;
    uint8_t peer_ip[4];
    uint16_t port;
    uint32_t poll_interval_ms;
    uint32_t rx_timeout_ms;
} wifi_sync_config_t;

uint8_t WifiSync_Init(const wifi_sync_config_t *config);
void    WifiSync_Process(uint32_t now_ms);
uint8_t WifiSync_PopEvent(wifi_sync_event_t *event);
uint8_t WifiSync_SendGameOver(uint8_t game);
uint8_t WifiSync_SendReset(uint8_t game);
uint8_t WifiSync_IsReady(void);
const char *WifiSync_GetLastError(void);
uint8_t WifiSync_IsConnected(void);
uint8_t WifiSync_GetDeviceCount(void);


#ifdef __cplusplus
}
#endif

#endif /* WIFI_SYNC_H */
