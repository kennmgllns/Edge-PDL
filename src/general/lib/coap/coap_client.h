/*
 * coap_client.h
 */

#ifndef __COAP_CLIENT_H__
#define __COAP_CLIENT_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Configurable Constants
 */

#define COAP_DEFAULT_PORT                 (5683)

#ifndef COAP_BUF_MAX_SIZE
  #define COAP_BUF_MAX_SIZE               (512) // should not be more than the dtls client buffer
#endif

#ifndef COAP_MAX_OPTION_NUM
  #define COAP_MAX_OPTION_NUM             (4) // up to 16
#endif


/*
 * Global Constants
 */

#define COAP_VERSION                      (1)
#define COAP_PAYLOAD_MARKER               (0xFF)

typedef enum
{
    COAP_TYPE_CONFIRMABLE                 = 0,
    COAP_TYPE_NON_CONFIRMABLE             = 1,
    COAP_TYPE_ACKNOWLEDGEMENT             = 2,
    COAP_TYPE_RESET                       = 3
} coap_msg_type_et;

typedef enum
{
    COAP_METHOD_GET                       = 1,
    COAP_METHOD_POST                      = 2,
    COAP_METHOD_PUT                       = 3,
    COAP_METHOD_DELETE                    = 4
} coap_method_et;

typedef enum
{
  #define RESPONSE_CODE(resp, detail)     ((resp << 5) | (detail))
    COAP_RESP_CREATED                     = RESPONSE_CODE(2, 1),
    COAP_RESP_DELETED                     = RESPONSE_CODE(2, 2),
    COAP_RESP_VALID                       = RESPONSE_CODE(2, 3),
    COAP_RESP_CHANGED                     = RESPONSE_CODE(2, 4),
    COAP_RESP_CONTENT                     = RESPONSE_CODE(2, 5),
    COAP_RESP_BAD_REQUEST                 = RESPONSE_CODE(4, 0),
    COAP_RESP_UNAUTHORIZED                = RESPONSE_CODE(4, 1),
    COAP_RESP_BAD_OPTION                  = RESPONSE_CODE(4, 2),
    COAP_RESP_FORBIDDEN                   = RESPONSE_CODE(4, 3),
    COAP_RESP_NOT_FOUNT                   = RESPONSE_CODE(4, 4),
    COAP_RESP_METHOD_NOT_ALLOWD           = RESPONSE_CODE(4, 5),
    COAP_RESP_NOT_ACCEPTABLE              = RESPONSE_CODE(4, 6),
    COAP_RESP_PRECONDITION_FAILED         = RESPONSE_CODE(4, 12),
    COAP_RESP_REQUEST_ENTITY_TOO_LARGE    = RESPONSE_CODE(4, 13),
    COAP_RESP_UNSUPPORTED_CONTENT_FORMAT  = RESPONSE_CODE(4, 15),
    COAP_RESP_INTERNAL_SERVER_ERROR       = RESPONSE_CODE(5, 0),
    COAP_RESP_NOT_IMPLEMENTED             = RESPONSE_CODE(5, 1),
    COAP_RESP_BAD_GATEWAY                 = RESPONSE_CODE(5, 2),
    COAP_RESP_SERVICE_UNAVALIABLE         = RESPONSE_CODE(5, 3),
    COAP_RESP_GATEWAY_TIMEOUT             = RESPONSE_CODE(5, 4),
    COAP_RESP_PROXYING_NOT_SUPPORTED      = RESPONSE_CODE(5, 5)
} coap_resp_code_et;

typedef enum
{
    COAP_OPT_IF_MATCH                     = 1,
    COAP_OPT_URI_HOST                     = 3,
    COAP_OPT_E_TAG                        = 4,
    COAP_OPT_IF_NONE_MATCH                = 5,
    COAP_OPT_URI_PORT                     = 7,
    COAP_OPT_LOCATION_PATH                = 8,
    COAP_OPT_URI_PATH                     = 11,
    COAP_OPT_CONTENT_FORMAT               = 12,
    COAP_OPT_MAX_AGE                      = 14,
    COAP_OPT_URI_QUERY                    = 15,
    COAP_OPT_ACCEPT                       = 17,
    COAP_OPT_LOCATION_QUERY               = 20,
    COAP_OPT_PROXY_URI                    = 35,
    COAP_OPT_PROXY_SCHEME                 = 39
} coap_option_num_et;

#if 0 // not used in this library (binary payloads only)
typedef enum
{
    COAP_FORMAT_NONE                      = -1,
    COAP_FORMAT_TEXT_PLAIN                = 0,
    COAP_FORMAT_APPLICATION_LINK_FORMAT   = 40,
    COAP_FORMAT_APPLICATION_XML           = 41,
    COAP_FORMAT_APPLICATION_OCTET_STREAM  = 42,
    COAP_FORMAT_APPLICATION_EXI           = 47,
    COAP_FORMAT_APPLICATION_JSON          = 50,
    COAP_FORMAT_APPLICATION_CBOR          = 60
} coap_content_format_et;
#endif

/*
 * Global Structs
 */

typedef struct __attribute__((__packed__))
{
    /* |ver|type|tokenlen| bitfields (LSB order on GCC) */
    uint8_t   ui4_tokenlen:4;       // token length
    uint8_t   ui2_type:2;           // message type
    uint8_t   ui2_version:2;        // version number
    /* 1-byte request|response */
    uint8_t   ui8_code;             // request method or response code
    /* 2-byte ID (MSB first) */
    uint8_t   ui8_msg_id_h;         // message ID high byte
    uint8_t   ui8_msg_id_l;         // message ID low byte
} coap_header_st; // 32-bit header

typedef struct
{
    uint8_t         e_option;       // coap_option_num_et
    uint8_t         ui8_len;        // option length
    const uint8_t  *pui8_ptr;       // pointer to buffer
} coap_option_st;

typedef struct
{
    coap_header_st  s_header;
    uint8_t         ui8_optioncount;
    coap_option_st  as_options[COAP_MAX_OPTION_NUM];
    const uint8_t  *pui8_token;
    const uint8_t  *pui8_payload;
    uint16_t        ui16_payloadlen;
} coap_packet_st; // packet info only, not the actual buffer


typedef struct
{
  int    (*fpi_send_handler)(const uint8_t *pui8_buf, uint16_t ui16_len);
  void   (*fpv_resp_handler)(coap_packet_st *ps_resp_packet);
  uint8_t   aui8_buffer[COAP_BUF_MAX_SIZE];
} coap_client_context_st;

bool coapClientHandleMsg(coap_client_context_st *ps_client_ctx, const uint8_t *pui8_msg, uint16_t ui16_msg_len);
bool coapClientSendRequest(coap_client_context_st *ps_client_ctx, coap_method_et e_method, uint16_t ui16_msg_id, const char *pc_path, const uint8_t *pui8_payload, uint16_t ui16_payloadlen);

#define coapClientInit(ctx, f_send, f_resp)   memset(&ctx, 0, sizeof(ctx));   \
                                              ctx.fpi_send_handler = f_send;  \
                                              ctx.fpv_resp_handler = f_resp
#define coapClientSendGetRequest(p_ctx, ...)  coapClientSendRequest(p_ctx, COAP_METHOD_GET, ## __VA_ARGS__)
#define coapClientSendPutRequest(p_ctx, ...)  coapClientSendRequest(p_ctx, COAP_METHOD_PUT, ## __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif
