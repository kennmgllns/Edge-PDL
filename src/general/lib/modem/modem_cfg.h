
#pragma once


// use UART2 for modem terminal
#define MODEM_UART_PORT                 (UART_NUM_2)
#define MODEM_UART_RXD_PIN              (GPIO_NUM_16)
#define MODEM_UART_TXD_PIN              (GPIO_NUM_17)
#define MODEM_UART_RX_FIFO_SIZE         (1024)

#define MODEM_COMMS_BUFFER_SIZE         (512 + 128)
#define MODEM_COMMS_RX_LINE_TIMEOUT     (150)       // 150ms
#define MODEM_COMMS_RX_WAIT_TIMEOUT     (5 * 1000)  // 5 seconds

#define MODEM_COMMS_AT_CMD_DEBUG        (false)      // debug print sent AT commands
#define MODEM_COMMS_AT_RSP_DEBUG        (false)      // debug print receive AT response

#define MODEM_NET_STAT_UPDATE_INTERVAL  (15 * 1000) // milliseconds interval of regular reading of rssi, etc.
#define MODEM_PDP_ADDR_CHECK_INTERVAL   (60 * 1000) // milliseconds interval of regular checking of IP address
#define MODEM_PDP_ACTIVATE_TIMEOUT      (150 * 1000)// timeout waiting for network connection
#define MODEM_PDP_MAX_FAIL_ATTEMPTS     (3 * 2)     // number of failed attempts before resetting the modem

#define MODEM_PDP_MAX_SESSIONS          (2)         // limit open sockets

#define MODEM_APN_MAX_STR_LENGTH        (63+1)      // max APN string length
