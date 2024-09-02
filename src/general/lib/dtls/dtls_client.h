
/**
 * @file dtls_client.h
 */

#ifndef _DTLS_CLIENT_H_
#define _DTLS_CLIENT_H_

#include <stdint.h>
#include <stdlib.h>

#include "dtls_defs.h"
#include "dtls_crypto.h"


#ifdef __cplusplus
extern "C" {
#endif

/* callback functions used to communicate with the application. */
typedef struct {
  /* send DTLS packets over the network. */
  int (*write)(uint8_t *buf, size_t len);
  /* decrypted application data that was received */
  int (*read)(uint8_t *buf, size_t len);
  /* event handler */
  int (*event)(dtls_alert_level_et level, unsigned short code);
  /* called during handshake related to the psk key exchange */
  int (*get_psk_info)(dtls_credentials_type_et type, const uint8_t *desc, size_t desc_len, uint8_t *result, size_t result_length);

} dtls_handler_st;

typedef enum { DTLS_CLIENT=0, DTLS_SERVER } dtls_peer_type;

/** DTLS client context */
typedef struct
{
  dtls_state_et                 e_state;                      // DTLS engine state
  dtls_handler_st               s_handler;                    // callback handlers
  dtls_handshake_parameters_st  s_handshake_params;
  dtls_security_parameters_st   as_security_params[2];
  dtls_security_parameters_st  *ps_active_security_param;
  uint8_t                       aui8_readbuf[DTLS_MAX_BUF];
} dtls_client_context_st;

/* initialize client context */
int dtls_client_init(dtls_client_context_st *p_ctx);
/* establishes a DTLS channel. */
int dtls_connect(dtls_client_context_st *ctx);
/* closes the DTLS connection */
int dtls_close(dtls_client_context_st *ctx);
/* reconnect */
int dtls_renegotiate(dtls_client_context_st *ctx);
/* writes the application data */
int dtls_write(dtls_client_context_st *ctx, const uint8_t *buf, size_t len);
/* handles incoming data as DTLS message */
int dtls_handle_message(dtls_client_context_st *ctx, uint8_t *msg, int msglen);

/** for backward compatibility */
#define dtlsClientInit2         dtls_client_init
#define dtlsClientConnect       dtls_connect
#define dtlsClientSend          dtls_write
#define dtlsClientClose         dtls_close
#define dtlsClientHandleMsg     dtls_handle_message


/** DTLS record layer. */
typedef struct __attribute__((__packed__))
{
  uint8_t u8_content_type;            // content type of the included message
  uint8_t u16_version[2];             // protocol version
  uint8_t u16_epoch[2];               //counter for cipher state changes
  uint8_t u48_sequence_number[6];     // sequence number
  uint8_t u16_length[2];              // length of the following fragment
  /* fragment */
} dtls_record_header_st;

/** DTLS handshake protocol header */
typedef struct __attribute__((__packed__))
{
  uint8_t u8_msg_type;                // one of dtls_handshake_type_et
  uint8_t u24_length[3];              // length of this message
  uint8_t u16_message_seq[2];         // message sequence number
  uint8_t u24_fragment_offset[3];     // fragment offset
  uint8_t u24_fragment_length[3];     // fragment length
  /* body */
} dtls_handshake_header_st;

/** client hello message */
typedef struct __attribute__((__packed__))
{
  uint8_t u16_version[2];             // client version
  uint8_t u32_gmt_random[4];          // GMT time of the random byte creation
  uint8_t aui8_random[28];            // client random bytes
  /* session id (up to 32 bytes) */
  /* cookie (up to 32 bytes) */
  /* cipher suite (2 to 2^16 -1 bytes) */
  /* compression method */
} dtls_client_hello_st;

/** hello verify request */
typedef struct __attribute__((__packed__))
{
  uint8_t u16_version[2];             // server version
  uint8_t u8_cookie_length;           // length of the included cookie
  uint8_t au8_cookie[];               // up to 32 bytes making up the cookie
} dtls_hello_verify_st;


#ifdef __cplusplus
}
#endif

#endif /* _DTLS_CLIENT_H_ */
