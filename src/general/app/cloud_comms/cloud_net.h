
#pragma once

namespace cloud
{

/*
 * Global Constants
 */
typedef enum
{
    NET_STATE_IDLE,
    NET_STATE_SEND_REQ,
    NET_STATE_WAIT_RESP,
    NET_STATE_RETRY_DELAY,
    NET_STATE_GET_COMMAND,
    NET_STATE_SEND_RESP,
    NET_STATE_WAIT_STAT_RESP,
} net_state_et;

typedef struct
{
    net_state_et    e_state;            // task state
    uint16_t        ui16_message_id;    // coap message id
    bool            b_resp_received;    // true = got response from server
    bool            b_resp_status;      // true = response is valid
    uint32_t        ms_resp_timeout;    // millisecond timestamp of last request sent
    uint32_t        ms_retry_delay;     // millisecond timestamp of last failed request
    uint8_t         ui8_resp_errors;    // number of server error responses
} net_context_et; // coap-comms sub-tasks status


namespace net
{

/*
 * Global Definitions
 */
typedef bool (*send_func_pt)(const uint8_t *pui8_buff, size_t sz_len);

/*
 * Public Function Prototypes
 */
void init(send_func_pt fpb_send_handler);
uint16_t newMessageId(void);

// coap
bool sendGetRequest(uint16_t ui16_msg_id, const char *pc_path, const uint8_t *pui8_payload, size_t sz_payload_len);
bool sendPutRequest(uint16_t ui16_msg_id, const char *pc_path, const uint8_t *pui8_payload, size_t sz_payload_len);
bool handleMessage(const uint8_t *pui8_buf, size_t sz_buf_len);
bool parseServerResponse(const uint8_t *pui8_buf, size_t sz_len);

// stats
void timeoutOccured(void);
bool getStats(uint32_t *pui32_sent, uint32_t *pui32_recv, uint32_t *pui32_timeout);
void resetStats(void);

} // namespace cloud::net


/* cloud_sync.c */
namespace sync
{
void init(void);
void cycle(void);
void handleResponse(uint16_t ui16_message_id, const uint8_t *pui8_payload_buf, size_t sz_payload_len);
bool getStatus(void); // time sync status
} // namespace cloud::sync

/* cloud_status.cpp */
namespace status
{
void init(void);
void cycle(void);
void parseResponse(uint16_t ui16_message_id, const uint8_t *pui8_payload_buf, size_t sz_payload_len);
} // namespace cloud::sync

/* cloud_monitor.cpp */
namespace monitor
{
void init(void);
void cycle(void);
void parseResponse(uint16_t ui16_message_id, const uint8_t *pui8_payload_buf, size_t sz_payload_len);
} // namespace cloud::monitor

/* cloud_event.cpp */
namespace event
{
void init(void);
void cycle(void);
void parseResponse(uint16_t ui16_message_id, const uint8_t *pui8_payload_buf, size_t sz_payload_len);
} // namespace cloud::event

/* cloud_update.cpp */
namespace update
{
void init(void);
void cycle(void);
void parseResponse(uint16_t ui16_message_id, const uint8_t *pui8_payload_buf, size_t sz_payload_len);
void requestQueued(void);
} // namespace cloud::update

/* cloud_commands.cpp */
namespace commands
{
void init(void);
void cycle(void);
void parseResponse(uint16_t ui16_message_id, const uint8_t *pui8_payload_buf, size_t sz_payload_len);
void requestQueued(void);
bool getResetRequestStatus(void);
} // namespace cloud::commands

} // namespace cloud
