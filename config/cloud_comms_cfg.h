
#pragma once


// #define K_CLOUD_HOST_ADDR                   "gdl.mnl-p-syd.edgeconnected.com"
#define K_CLOUD_HOST_ADDR                   "gdl.mnl-t-sig.edgeconnected.com"           // staging server

/* default coap-secured port */
#define K_CLOUD_HOST_PORT                   (5684)

/* local IPv4 address of the server via VPN */
#define K_CLOUD_PRIVATE_HOST_ADDR           "172.31.24.32"          // same server instance with prod server
#define K_CLOUD_PRIVATE_APNAME              "edgeelectrons.com"     // private APN (e.g. for Vodafone Default_CSP SIM's)

#define K_CLOUD_DTLS_PSK_ID                 "gdl"
#define K_CLOUD_DTLS_PSK_KEY                "m6G2jpN4Z9c3BRFe"

#define K_CLOUD_DTLS_CONN_RETRY_LIM         (5)                     // 5 times dtls connect retry limit
#define K_CLOUD_DTLS_CONN_TIMEOUT           (30 * 1000UL)           // 30-second dtls connect timeout

/* coap uri's */
#define K_CLOUD_TIME_PATH                   "time"
#define K_CLOUD_HEARTBEAT_PATH              "hb"
#define K_CLOUD_MONITOR_PATH                "mn"
#define K_CLOUD_STATUS_PATH                 "st"
#define K_CLOUD_EVENT_PATH                  "ev"
#define K_CLOUD_UPDATE_PATH                 "ud"

/* coap response */
#define K_CLOUD_RESPONSE_TIMEOUT_RETRY_LIM  (5)                     // 5 times timeout retry limit before reconnecting
#define K_CLOUD_RESPONSE_ERROR_RETRY_LIM    (10)                    // 10 times got server error response

/* coap-sync */
#define K_CLOUD_SYNC_INTERVAL               (3 * 60 * 60 * 1000)    // sync time every 3 hour
#define K_CLOUD_SYNC_RETRY_DELAY            (10 * 1000)             // 10s sync retry delay
#define K_CLOUD_SYNC_RESPONSE_TIMEOUT       (30 * 1000)             // 30s sync response timeout
#define K_CLOUD_SYNC_VALID_RESPONSE_DELAY   (5)                     // 5s sync response valid delay
#define K_VALID_EPOCH_TS                    (1500000000)            // 2017