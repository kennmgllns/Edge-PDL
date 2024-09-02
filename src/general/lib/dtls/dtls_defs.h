
/**
 * @file dtls_defs.h
 */

#ifndef _DTLS_DEFS_H_
#define _DTLS_DEFS_H_

#define DTLS1_VERSION           0xFEFF   /* DTLS v1.0 */
#define DTLS1_2_VERSION         0xFEFD   /* DTLS v1.2 */
#define DTLS_VERSION            DTLS1_2_VERSION

/*
 * Configurable Constants
 */
#define DTLS_MAX_BUF            (512 + 64)   // up to 1400 bytes, but should be less than the modem-uart buffer size
#define DTLS_COOKIE_LENGTH      16


/*
 * DTLS Constants
 */

typedef enum
{
  DTLS_CT_CHANGE_CIPHER_SPEC    = 20,
  DTLS_CT_ALERT                 = 21,
  DTLS_CT_HANDSHAKE             = 22,
  DTLS_CT_APPLICATION_DATA      = 23
} dtls_content_type_et;

/* Handshake types */
typedef enum
{
  DTLS_HT_HELLO_REQUEST         = 0,
  DTLS_HT_CLIENT_HELLO          = 1,
  DTLS_HT_SERVER_HELLO          = 2,
  DTLS_HT_HELLO_VERIFY_REQUEST  = 3,
  DTLS_HT_CERTIFICATE           = 11,
  DTLS_HT_SERVER_KEY_EXCHANGE   = 12,
  DTLS_HT_CERTIFICATE_REQUEST   = 13,
  DTLS_HT_SERVER_HELLO_DONE     = 14,
  DTLS_HT_CERTIFICATE_VERIFY    = 15,
  DTLS_HT_CLIENT_KEY_EXCHANGE   = 16,
  DTLS_HT_FINISHED              = 20
} dtls_handshake_type_et;

typedef enum
{
  DTLS_ALERT_LEVEL_WARNING=1,
  DTLS_ALERT_LEVEL_FATAL=2
} dtls_alert_level_et;

typedef enum
{
  DTLS_ALERT_CLOSE_NOTIFY             = 0,    // close_notify
  DTLS_ALERT_UNEXPECTED_MESSAGE       = 10,   // unexpected_message
  DTLS_ALERT_BAD_RECORD_MAC           = 20,   // bad_record_mac
  DTLS_ALERT_RECORD_OVERFLOW          = 22,   // record_overflow
  DTLS_ALERT_DECOMPRESSION_FAILURE    = 30,   // decompression_failure
  DTLS_ALERT_HANDSHAKE_FAILURE        = 40,   // handshake_failure
  DTLS_ALERT_BAD_CERTIFICATE          = 42,   // bad_certificate
  DTLS_ALERT_UNSUPPORTED_CERTIFICATE  = 43,   // unsupported_certificate
  DTLS_ALERT_CERTIFICATE_REVOKED      = 44,   // certificate_revoked
  DTLS_ALERT_CERTIFICATE_EXPIRED      = 45,   // certificate_expired
  DTLS_ALERT_CERTIFICATE_UNKNOWN      = 46,   // certificate_unknown
  DTLS_ALERT_ILLEGAL_PARAMETER        = 47,   // illegal_parameter
  DTLS_ALERT_UNKNOWN_CA               = 48,   // unknown_ca
  DTLS_ALERT_ACCESS_DENIED            = 49,   // access_denied
  DTLS_ALERT_DECODE_ERROR             = 50,   // decode_error
  DTLS_ALERT_DECRYPT_ERROR            = 51,   // decrypt_error
  DTLS_ALERT_PROTOCOL_VERSION         = 70,   // protocol_version
  DTLS_ALERT_INSUFFICIENT_SECURITY    = 71,   // insufficient_security
  DTLS_ALERT_INTERNAL_ERROR           = 80,   // internal_error
  DTLS_ALERT_USER_CANCELED            = 90,   // user_canceled
  DTLS_ALERT_NO_RENEGOTIATION         = 100,  // no_renegotiation
  DTLS_ALERT_UNSUPPORTED_EXTENSION    = 110   // unsupported_extension
} dtls_alert_et;

typedef enum
{
  DTLS_EVENT_CONNECT                  = 0x01DC,   // initiated handshake
  DTLS_EVENT_CONNECTED                = 0x01DE,   // handshake or re-negotiation has finished
  DTLS_EVENT_RENEGOTIATE              = 0x01DF    // re-negotiation has started
} dtls_event_type_et;

#define dtls_alert_create(level, desc)      (-((level << 8) | desc))
#define dtls_alert_fatal_create(desc)       dtls_alert_create(DTLS_ALERT_LEVEL_FATAL, desc)

typedef enum 
{
  DTLS_PSK_HINT,
  DTLS_PSK_IDENTITY,
  DTLS_PSK_KEY
} dtls_credentials_type_et;

typedef enum
{ 
  DTLS_STATE_INIT = 0,
  DTLS_STATE_WAIT_CLIENTHELLO,
  DTLS_STATE_WAIT_CLIENTCERTIFICATE,
  DTLS_STATE_WAIT_CLIENTKEYEXCHANGE,
  DTLS_STATE_WAIT_CERTIFICATEVERIFY,
  DTLS_STATE_WAIT_CHANGECIPHERSPEC,
  DTLS_STATE_WAIT_FINISHED,
  DTLS_STATE_FINISHED, 
  /* client states */
  DTLS_STATE_CLIENTHELLO,
  DTLS_STATE_WAIT_SERVERCERTIFICATE,
  DTLS_STATE_WAIT_SERVERKEYEXCHANGE,
  DTLS_STATE_WAIT_SERVERHELLODONE,

  DTLS_STATE_CONNECTED,
  DTLS_STATE_CLOSING,
  DTLS_STATE_CLOSED
} dtls_state_et;


#endif // _DTLS_DEFS_H_
