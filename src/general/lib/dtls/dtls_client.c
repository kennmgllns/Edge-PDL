/**
 * @file dtls.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "global_defs.h"
#include "dtls_client.h"


#if 0
  #define dtls_debug                        LOGD
  #define dtls_info                         LOGI
  #define dtls_debug_dump(name, p, sz)      LOGD(name); LOGB(p, sz)
  #define dtls_debug_hexdump(name, p, sz)   LOGD(name); LOGB(p, sz)
  #define dtls_warn     LOGW
  #define dtls_crit     LOGE
  #define dtls_alert    LOGE
#else
  #define dtls_debug(...)
  #define dtls_info(...)
  #define dtls_debug_dump(...)
  #define dtls_debug_hexdump(...)
  #define dtls_warn(...)
  #define dtls_crit(...)
  #define dtls_alert(...)
#endif





/* numerics */
static inline int dtls_int_to_uint16(uint8_t *field, uint16_t value)
{
  field[0] = (value >> 8) & 0xff;
  field[1] = value & 0xff;
  return 2;
}

static inline int dtls_int_to_uint24(uint8_t *field, uint32_t value)
{
  field[0] = (value >> 16) & 0xff;
  field[1] = (value >> 8) & 0xff;
  field[2] = value & 0xff;
  return 3;
}

static inline int dtls_int_to_uint48(uint8_t *field, uint64_t value)
{
  field[0] = (value >> 40) & 0xff;
  field[1] = (value >> 32) & 0xff;
  field[2] = (value >> 24) & 0xff;
  field[3] = (value >> 16) & 0xff;
  field[4] = (value >> 8) & 0xff;
  field[5] = value & 0xff;
  return 6;
}

static inline uint16_t dtls_uint16_to_int(const uint8_t *field)
{
  return ((uint16_t)field[0] << 8)
	 | (uint16_t)field[1];
}

static inline int dtls_prng(uint8_t *buf, size_t len) {
  while (len--)
    *buf++ = rand() & 0xFF;
  return 1;
}


#define dtls_get_epoch(H) dtls_uint16_to_int((H)->u16_epoch)


#define DTLS_RH_LENGTH sizeof(dtls_record_header_st)
#define DTLS_HS_LENGTH sizeof(dtls_handshake_header_st)
#define DTLS_CH_LENGTH sizeof(dtls_client_hello_st) /* no variable length fields! */
#define DTLS_COOKIE_LENGTH_MAX 32
#define DTLS_CH_LENGTH_MAX sizeof(dtls_client_hello_st) + DTLS_COOKIE_LENGTH_MAX + 12 + 26
#define DTLS_HV_LENGTH sizeof(dtls_hello_verify_st)


#define DTLS_SKEXECPSK_LENGTH_MIN 2
#define DTLS_CKXPSK_LENGTH_MIN 2
#define DTLS_CKXEC_LENGTH (1 + 1 + DTLS_EC_KEY_SIZE + DTLS_EC_KEY_SIZE)
#define DTLS_FIN_LENGTH 12

#define HS_HDR_LENGTH  DTLS_RH_LENGTH + DTLS_HS_LENGTH


#define DTLS_RECORD_HEADER(M) ((dtls_record_header_st *)(M))
#define DTLS_HANDSHAKE_HEADER(M) ((dtls_handshake_header_st *)(M))

#define HANDSHAKE(M) ((dtls_handshake_header_st *)((M) + DTLS_RH_LENGTH))
#define CLIENTHELLO(M) ((dtls_client_hello_st *)((M) + HS_HDR_LENGTH))


/* some constants for the PRF */
#define PRF_LABEL(Label) prf_label_##Label
#define PRF_LABEL_SIZE(Label) (sizeof(PRF_LABEL(Label)) - 1)

static const uint8_t prf_label_master[] = "master secret";
static const uint8_t prf_label_key[] = "key expansion";
static const uint8_t prf_label_client[] = "client";
static const uint8_t prf_label_server[] = "server";
static const uint8_t prf_label_finished[] = " finished";

/* Calls cb_alert() with given arguments if defined, otherwise an
 * error message is logged and the result is -1. This is just an
 * internal helper.
 */
#define CALL(Context, which, ...)       ((Context)->s_handler.which ? (Context)->s_handler.which(__VA_ARGS__) : -1)

static int dtls_send_multi(dtls_client_context_st *ctx,
                dtls_security_parameters_st *security,
                uint8_t type, uint8_t *buf_array[],
                size_t buf_len_array[], size_t buf_array_len);

/**
 * Sends the fragment of length \p buflen given in \p buf to the
 * specified \p peer. The data will be MAC-protected and encrypted
 * according to the selected cipher and split into one or more DTLS
 * records of the specified \p type. This function returns the number
 * of bytes that were sent, or \c -1 if an error occurred.
 *
 * \param ctx    The DTLS context to use.
 * \param type   The content type of the record.
 * \param buf    The data to send.
 * \param buflen The actual length of \p buf.
 * \return Less than zero on error, the number of bytes written otherwise.
 */
static int dtls_send(dtls_client_context_st *ctx, uint8_t type, uint8_t *buf, size_t buflen)
{
  return dtls_send_multi(ctx, ctx->ps_active_security_param, type, &buf, &buflen, 1);
}

int dtls_write(dtls_client_context_st *ctx, const uint8_t *buf, size_t len)
{
  if (DTLS_STATE_CONNECTED != ctx->e_state) {
    return 0;
  } else {
    return dtls_send(ctx, DTLS_CT_APPLICATION_DATA, (uint8_t *)buf, len);
  }
}

/* used to check if a received datagram contains a DTLS message */
static char const content_types[] = {
  DTLS_CT_CHANGE_CIPHER_SPEC,
  DTLS_CT_ALERT,
  DTLS_CT_HANDSHAKE,
  DTLS_CT_APPLICATION_DATA,
  0                                 /* end marker */
};

/**
 * Checks if the content type of \p msg is known. This function returns
 * the found content type, or 0 otherwise.
 */
static int known_content_type(const uint8_t *msg)
{
  unsigned int n;
  assert(msg);

  for (n = 0; (content_types[n] != 0) && (content_types[n]) != msg[0]; n++)
    ;
  return content_types[n];
}

/**
 * Checks if \p msg points to a valid DTLS record
 */
static unsigned int is_record(uint8_t *msg, size_t msglen)
{
  unsigned int rlen = 0;

  if (msglen >= DTLS_RH_LENGTH) { /* FIXME allow empty records? */
    uint16_t version = dtls_uint16_to_int(msg + 1);
    if ((((version == DTLS1_2_VERSION) || (version == DTLS1_VERSION))
         && known_content_type(msg))) {
        rlen = DTLS_RH_LENGTH +
        dtls_uint16_to_int(DTLS_RECORD_HEADER(msg)->u16_length);

      /* we do not accept wrong length field in record header */
      if (rlen > msglen)
        rlen = 0;
    }
  }

  return rlen;
}

/**
 * Initializes \p buf as record header. The caller must ensure that \p
 * buf is capable of holding at least \c sizeof(dtls_record_header_st)
 * bytes. Increments sequence number counter of \p security.
 * \return pointer to the next byte after the written header.
 * The length will be set to 0 and has to be changed before sending.
 */
static inline uint8_t *dtls_set_record_header(uint8_t type, dtls_security_parameters_st *security, uint8_t *buf)
{
  dtls_record_header_st *hdr;

  hdr = (dtls_record_header_st *)buf;
  hdr->u8_content_type = type;

  dtls_int_to_uint16(hdr->u16_version, DTLS_VERSION);

  if (security) {
    dtls_int_to_uint16(hdr->u16_epoch, security->epoch);
    /* increment record sequence counter by 1 */
    dtls_int_to_uint48(hdr->u48_sequence_number, security->rseq++);
  } else {
    memset(hdr->u16_epoch, 0, sizeof(hdr->u16_epoch));
    memset(hdr->u48_sequence_number, 0, sizeof(hdr->u48_sequence_number));
  }

  memset(hdr->u16_length, 0, sizeof(hdr->u16_length));
  return buf + sizeof(dtls_record_header_st);
}

/**
 * Initializes \p buf as handshake header. The caller must ensure that \p
 * buf is capable of holding at least \c sizeof(dtls_handshake_header_st)
 * bytes. Increments message sequence number counter of \p peer.
 * \return pointer to the next byte after \p buf
 */
static inline uint8_t *dtls_set_handshake_header(dtls_client_context_st *ctx, uint8_t type,
                                               int length, int frag_offset, int frag_length, uint8_t *buf)
{
  dtls_handshake_header_st *hdr;

  hdr = (dtls_handshake_header_st *)buf;
  hdr->u8_msg_type = type;

  dtls_int_to_uint24(hdr->u24_length, length);

  /* increment handshake message sequence counter by 1 */
  dtls_int_to_uint16(hdr->u16_message_seq, ctx->s_handshake_params.hs_state.mseq_s++);

  dtls_int_to_uint24(hdr->u24_fragment_offset, frag_offset);
  dtls_int_to_uint24(hdr->u24_fragment_length, frag_length);

  return buf + sizeof(dtls_handshake_header_st);
}

/** returns true if the cipher matches TLS_PSK_WITH_AES_128_CCM_8 */
static inline int is_tls_psk_with_aes_128_ccm_8(dtls_cipher_et e_cipher)
{
  return e_cipher == TLS_PSK_WITH_AES_128_CCM_8;
}

/** returns true if the application is configured for psk */
static inline int is_psk_supported(dtls_client_context_st *ctx)
{
  return ctx && ctx->s_handler.get_psk_info;
}

/**
 * Returns @c 1 if @p code is a cipher suite other than @c
 * TLS_NULL_WITH_NULL_NULL that we recognize.
 *
 * @param ctx   The current DTLS context
 * @param code The cipher suite identifier to check
 * @param is_client 1 for a dtls client, 0 for server
 * @return @c 1 iff @p code is recognized,
 */
static int known_cipher(dtls_client_context_st *ctx, dtls_cipher_et code, int is_client)
{
  int psk;
  psk = is_psk_supported(ctx);
  return (psk && is_tls_psk_with_aes_128_ccm_8(code));
}

/**
 * This method detects if we already have a established DTLS session with
 * peer and the peer is attempting to perform a fresh handshake by sending
 * messages with epoch = 0. This is to handle situations mentioned in
 * RFC 6347 - section 4.2.8.
 *
 * @param msg  The packet received from Client
 * @param msglen Packet length
 * @param peer peer who is the sender for this packet
 * @return @c 1 if this is a rehandshake attempt by
 * client
 */
static int hs_attempt_with_existing_peer(uint8_t *msg, size_t msglen, dtls_client_context_st *ctx)
{
    if ((ctx) && (DTLS_STATE_CONNECTED == ctx->e_state)) {
      if (msg[0] == DTLS_CT_HANDSHAKE) {
        uint16_t msg_epoch = dtls_uint16_to_int(DTLS_RECORD_HEADER(msg)->u16_epoch);
        if (msg_epoch == 0) {
          dtls_handshake_header_st * hs_header = DTLS_HANDSHAKE_HEADER(msg + DTLS_RH_LENGTH);
          if (hs_header->u8_msg_type == DTLS_HT_CLIENT_HELLO ||
              hs_header->u8_msg_type == DTLS_HT_HELLO_REQUEST) {
            return 1;
          }
        }
      }
    }
    return 0;
}

static inline dtls_security_parameters_st *dtls_security_params_next(dtls_client_context_st *ctx)
{
  if (ctx->ps_active_security_param == &ctx->as_security_params[0]) {
    dtls_security_init(&ctx->as_security_params[1]);
    ctx->as_security_params[1].epoch = ctx->as_security_params[0].epoch + 1;
    return &ctx->as_security_params[1];
  }
  else {
    dtls_security_init(&ctx->as_security_params[0]);
    ctx->as_security_params[0].epoch = ctx->as_security_params[1].epoch + 1;
    return &ctx->as_security_params[0];
  }
}

/**
 * Calculate the pre master secret and after that calculate the master-secret.
 */
static int calculate_key_block(dtls_client_context_st *ctx, dtls_handshake_parameters_st *handshake, dtls_peer_type role)
{
  uint8_t *pre_master_secret;
  int pre_master_len = 0;
  dtls_security_parameters_st *security = dtls_security_params_next(ctx);
  uint8_t master_secret[DTLS_MASTER_SECRET_LENGTH];

  if (!security) {
    return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
  }

  pre_master_secret = security->key_block;

  switch (handshake->e_cipher) {
  case TLS_PSK_WITH_AES_128_CCM_8: {
    uint8_t psk[DTLS_PSK_MAX_KEY_LEN];
    int len;

    len = CALL(ctx, get_psk_info, DTLS_PSK_KEY,
               handshake->keyx.psk.identity,
               handshake->keyx.psk.id_length,
               psk, DTLS_PSK_MAX_KEY_LEN);
    if (len < 0) {
      dtls_crit("no psk key for session available");
      return len;
    }
  /* Temporarily use the key_block storage space for the pre master secret. */
    pre_master_len = dtls_psk_pre_master_secret(psk, len,
                                                pre_master_secret,
                                                MAX_KEYBLOCK_LENGTH);

    dtls_debug_hexdump("psk", psk, len);

    memset(psk, 0, DTLS_PSK_MAX_KEY_LEN);
    if (pre_master_len < 0) {
      dtls_crit("the psk was too long, for the pre master secret");
      return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
    }

    break;
  }
  default:
    dtls_crit("calculate_key_block: unknown cipher");
    return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
  }

  dtls_debug_dump("client_random", handshake->tmp.random.client, DTLS_RANDOM_LENGTH);
  dtls_debug_dump("server_random", handshake->tmp.random.server, DTLS_RANDOM_LENGTH);
  dtls_debug_dump("pre_master_secret", pre_master_secret, pre_master_len);

  dtls_prf(pre_master_secret, pre_master_len,
           PRF_LABEL(master), PRF_LABEL_SIZE(master),
           handshake->tmp.random.client, DTLS_RANDOM_LENGTH,
           handshake->tmp.random.server, DTLS_RANDOM_LENGTH,
           master_secret,
           DTLS_MASTER_SECRET_LENGTH);

  dtls_debug_dump("master_secret", master_secret, DTLS_MASTER_SECRET_LENGTH);

  /* create key_block from master_secret
   * key_block = PRF(master_secret,
                    "key expansion" + tmp.random.server + tmp.random.client) */

  dtls_prf(master_secret,
           DTLS_MASTER_SECRET_LENGTH,
           PRF_LABEL(key), PRF_LABEL_SIZE(key),
           handshake->tmp.random.server, DTLS_RANDOM_LENGTH,
           handshake->tmp.random.client, DTLS_RANDOM_LENGTH,
           security->key_block, MAX_KEYBLOCK_LENGTH);

  memcpy(handshake->tmp.master_secret, master_secret, DTLS_MASTER_SECRET_LENGTH);

  security->e_cipher = handshake->e_cipher;
  security->e_compression = handshake->e_compression;
  security->rseq = 0;

  return 0;
}

/**
 * Parse the ClientKeyExchange and update the internal handshake state with
 * the new data.
 */
static inline int check_client_keyexchange(dtls_client_context_st *ctx,
                         dtls_handshake_parameters_st *handshake, uint8_t *data, size_t length)
{

  if (is_tls_psk_with_aes_128_ccm_8(handshake->e_cipher)) {
    int id_length;

    if (length < DTLS_HS_LENGTH + DTLS_CKXPSK_LENGTH_MIN) {
      dtls_debug("The client key exchange is too short");
      return dtls_alert_fatal_create(DTLS_ALERT_HANDSHAKE_FAILURE);
    }
    data += DTLS_HS_LENGTH;

    id_length = dtls_uint16_to_int(data);
    data += sizeof(uint16_t);

    if (DTLS_HS_LENGTH + DTLS_CKXPSK_LENGTH_MIN + id_length != length) {
      dtls_debug("The identity has a wrong length");
      return dtls_alert_fatal_create(DTLS_ALERT_HANDSHAKE_FAILURE);
    }

    if (id_length > DTLS_PSK_MAX_CLIENT_IDENTITY_LEN) {
      dtls_warn("please use a smaller client identity");
      return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
    }

    handshake->keyx.psk.id_length = id_length;
    memcpy(handshake->keyx.psk.identity, data, id_length);
  }

  return 0;
}

static inline void update_hs_hash(dtls_client_context_st *ctx, uint8_t *data, size_t length)
{
  dtls_debug_dump("add MAC data", data, length);
  sha256Update(&ctx->s_handshake_params.hs_state.hs_hash, data, length);
}

static void copy_hs_hash(dtls_client_context_st *ctx, sha256_context_st *hs_hash)
{
  memcpy(hs_hash, &ctx->s_handshake_params.hs_state.hs_hash,
         sizeof(ctx->s_handshake_params.hs_state.hs_hash));
}

static inline size_t finalize_hs_hash(dtls_client_context_st *ctx, uint8_t *buf)
{
  sha256Finalize(&ctx->s_handshake_params.hs_state.hs_hash, buf);
  return SHA256_DIGEST_LENGTH;
}

static inline void clear_hs_hash(dtls_client_context_st *ctx)
{
  dtls_debug("clear MAC");
  sha256Init(&ctx->s_handshake_params.hs_state.hs_hash);
}

/**
 * Checks if \p record + \p data contain a Finished message with valid
 * verify_data.
 *
 * \param ctx    The current DTLS context.
 * \param peer   The remote peer of the security association.
 * \param data   The cleartext payload of the message.
 * \param data_length Actual length of \p data.
 * \return \c 0 if the Finished message is valid, \c negative number otherwise.
 */
static int check_finished(dtls_client_context_st *ctx, uint8_t *data, size_t data_length)
{
  size_t digest_length, label_size;
  const uint8_t *label;
  uint8_t buf[DTLS_HMAC_MAX];

  if (data_length < DTLS_HS_LENGTH + DTLS_FIN_LENGTH)
    return dtls_alert_fatal_create(DTLS_ALERT_HANDSHAKE_FAILURE);

  /* Use a union here to ensure that sufficient stack space is
   * reserved. As statebuf and verify_data are not used at the same
   * time, we can re-use the storage safely.
   */
  union {
    uint8_t statebuf[DTLS_HASH_CTX_SIZE];
    uint8_t verify_data[DTLS_FIN_LENGTH];
  } b;

  /* temporarily store hash status for roll-back after finalize */
  memcpy(b.statebuf, &ctx->s_handshake_params.hs_state.hs_hash, DTLS_HASH_CTX_SIZE);

  digest_length = finalize_hs_hash(ctx, buf);
  /* clear_hash(); */

  /* restore hash status */
  memcpy(&ctx->s_handshake_params.hs_state.hs_hash, b.statebuf, DTLS_HASH_CTX_SIZE);

  label = PRF_LABEL(server);
  label_size = PRF_LABEL_SIZE(server);

  dtls_prf(ctx->s_handshake_params.tmp.master_secret,
           DTLS_MASTER_SECRET_LENGTH,
           label, label_size,
           PRF_LABEL(finished), PRF_LABEL_SIZE(finished),
           buf, digest_length,
           b.verify_data, sizeof(b.verify_data));

  dtls_debug_dump("d:", data + DTLS_HS_LENGTH, sizeof(b.verify_data));
  dtls_debug_dump("v:", b.verify_data, sizeof(b.verify_data));

  /* compare verify data and create DTLS alert code when they differ */
  return (0 == memcmp(data + DTLS_HS_LENGTH, b.verify_data, sizeof(b.verify_data))) ? 0
    : dtls_alert_create(DTLS_ALERT_LEVEL_FATAL, DTLS_ALERT_HANDSHAKE_FAILURE);
}

/**
 * Prepares the payload given in \p data for sending with dtls_send().
 * \param security  The encryption paramater used to encrypt
 * \param type    The content type of this record.
 * \param data_array Array with payloads in correct order.
 * \param data_len_array sizes of the payloads in correct order.
 * \param data_array_len The number of payloads given.
 * \param sendbuf The output buffer where the encrypted record will be placed.
 * \param rlen    This parameter must be initialized with the
 *                maximum size of \p sendbuf and will be updated
 *                to hold the actual size of the stored packet
 *                on success. On error, the value of \p rlen is
 *                undefined.
 * \return Less than zero on error, or greater than zero success.
 */
static int dtls_prepare_record(dtls_client_context_st *ctx, dtls_security_parameters_st *security,
                    uint8_t type, uint8_t *data_array[], size_t data_len_array[],
                    size_t data_array_len, uint8_t *sendbuf, size_t *rlen)
{
  uint8_t *p, *start;
  int res;
  unsigned int i;

  if (*rlen < DTLS_RH_LENGTH) {
    dtls_alert("The sendbuf (%zu bytes) is too small", *rlen);
    return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
  }

  p = dtls_set_record_header(type, security, sendbuf);
  start = p;

  if (!security || security->e_cipher == TLS_NULL_WITH_NULL_NULL) {
    /* no cipher suite */

    res = 0;
    for (i = 0; i < data_array_len; i++) {
      /* check the minimum that we need for packets that are not encrypted */
      if (*rlen < res + DTLS_RH_LENGTH + data_len_array[i]) {
        dtls_debug("dtls_prepare_record: send buffer too small");
        return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
      }

      memcpy(p, data_array[i], data_len_array[i]);
      p += data_len_array[i];
      res += data_len_array[i];
    }
  } else { /* TLS_PSK_WITH_AES_128_CCM_8 or TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8 */
    /**
     * length of additional_data for the AEAD cipher which consists of
     * seq_num(2+6) + type(1) + version(2) + length(2)
     */
#define A_DATA_LEN 13
    uint8_t nonce[DTLS_CCM_BLOCKSIZE];
    uint8_t A_DATA[A_DATA_LEN];

    if (is_tls_psk_with_aes_128_ccm_8(security->e_cipher)) {
      dtls_debug("dtls_prepare_record(): encrypt using TLS_PSK_WITH_AES_128_CCM_8");
    } else {
      dtls_debug("dtls_prepare_record(): encrypt using unknown cipher");
    }

    /* set nonce
       from RFC 6655:
           The "nonce" input to the AEAD algorithm is exactly that of [RFC5288]:
           the "nonce" SHALL be 12 bytes long and is constructed as follows:
           (this is an example of a "partially explicit" nonce; see Section
           3.2.1 in [RFC5116]).

                       struct {
             opaque salt[4];
             opaque nonce_explicit[8];
                       } CCMNonce;

         [...]

           In DTLS, the 64-bit seq_num is the 16-bit epoch concatenated with the
            48-bit seq_num.

            When the nonce_explicit is equal to the sequence number, the CCMNonce
            will have the structure of the CCMNonceExample given below.

                       struct {
                        uint32 client_write_IV; // low order 32-bits
                        uint64 seq_num;         // TLS sequence number
                       } CCMClientNonce.


                       struct {
                        uint32 server_write_IV; // low order 32-bits
                        uint64 seq_num; // TLS sequence number
                       } CCMServerNonce.


                       struct {
                        case client:
                          CCMClientNonce;
                        case server:
                          CCMServerNonce:
                       } CCMNonceExample;
    */

    memcpy(p, &DTLS_RECORD_HEADER(sendbuf)->u16_epoch, 8);
    p += 8;
    res = 8;

    for (i = 0; i < data_array_len; i++) {
      /* check the minimum that we need for packets that are not encrypted */
      if (*rlen < res + DTLS_RH_LENGTH + data_len_array[i]) {
        dtls_debug("dtls_prepare_record: send buffer too small");
        return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
      }

      memcpy(p, data_array[i], data_len_array[i]);
      p += data_len_array[i];
      res += data_len_array[i];
    }

    memset(nonce, 0, DTLS_CCM_BLOCKSIZE);
    memcpy(nonce, dtls_kb_local_iv(security, DTLS_CLIENT), DTLS_IV_LENGTH);
    memcpy(nonce + DTLS_IV_LENGTH, start, 8); /* epoch + seq_num */

    dtls_debug_dump("nonce:", nonce, DTLS_CCM_BLOCKSIZE);

    /* re-use N to create additional data according to RFC 5246, Section 6.2.3.3:
     *
     * additional_data = seq_num + TLSCompressed.type +
     *                   TLSCompressed.version + TLSCompressed.length;
     */
    memcpy(A_DATA, &DTLS_RECORD_HEADER(sendbuf)->u16_epoch, 8); /* epoch and seq_num */
    memcpy(A_DATA + 8,  &DTLS_RECORD_HEADER(sendbuf)->u8_content_type, 3); /* type and version */
    dtls_int_to_uint16(A_DATA + 11, res - 8); /* length */

    res = dtls_encrypt(&ctx->ps_active_security_param->s_cipher, start + 8, res - 8, start + 8, nonce,
                       dtls_kb_local_write_key(security, DTLS_CLIENT),
                       DTLS_KEY_LENGTH, A_DATA, A_DATA_LEN);

    if (res < 0)
      return res;

    res += 8;                        /* increment res by size of nonce_explicit */
    dtls_debug_dump("message:", start, res);
  }

  /* fix length of fragment in sendbuf */
  dtls_int_to_uint16(sendbuf + 11, res);

  *rlen = DTLS_RH_LENGTH + res;
  return 0;
}

static int dtls_send_handshake_msg_hash(dtls_client_context_st *ctx, uint8_t header_type,
                                        uint8_t *data, size_t data_length, int add_hash)
{
  uint8_t buf[DTLS_HS_LENGTH];
  uint8_t *data_array[2];
  size_t data_len_array[2];
  int i = 0;
  dtls_security_parameters_st *security = ctx->ps_active_security_param;

  dtls_set_handshake_header(ctx, header_type, data_length, 0, data_length, buf);

  if (add_hash) {
    update_hs_hash(ctx, buf, sizeof(buf));
  }
  data_array[i] = buf;
  data_len_array[i] = sizeof(buf);
  i++;

  if (data != NULL) {
    if (add_hash) {
      update_hs_hash(ctx, data, data_length);
    }
    data_array[i] = data;
    data_len_array[i] = data_length;
    i++;
  }
  dtls_debug("send handshake packet of type: %u", header_type);
  return dtls_send_multi(ctx, security, DTLS_CT_HANDSHAKE,
                         data_array, data_len_array, i);
}

static int dtls_send_handshake_msg(dtls_client_context_st *ctx, uint8_t header_type,
                                   uint8_t *data, size_t data_length)
{
  return dtls_send_handshake_msg_hash(ctx, header_type, data, data_length, 1);
}

static int dtls_send_multi(dtls_client_context_st *ctx, dtls_security_parameters_st *security,
                           uint8_t type, uint8_t *buf_array[], size_t buf_len_array[], size_t buf_array_len)
{

  uint8_t sendbuf[DTLS_MAX_BUF];
  size_t len = sizeof(sendbuf);
  int res;
  unsigned int i;
  size_t overall_len = 0;

  res = dtls_prepare_record(ctx, security, type, buf_array, buf_len_array, buf_array_len, sendbuf, &len);

  if (res < 0)
    return res;


  dtls_debug_hexdump("send header", sendbuf, sizeof(dtls_record_header_st));
  for (i = 0; i < buf_array_len; i++) {
    dtls_debug_hexdump("send unencrypted", buf_array[i], buf_len_array[i]);
    overall_len += buf_len_array[i];
  }

  /* FIXME: copy to peer's sendqueue (after fragmentation if
   * necessary) and initialize retransmit timer */
  res = CALL(ctx, write, sendbuf, len);

  /* Guess number of bytes application data actually sent:
   * dtls_prepare_record() tells us in len the number of bytes to
   * send, res will contain the bytes actually sent. */
  return res <= 0 ? res : overall_len - (len - res);
}

static inline int dtls_send_alert(dtls_client_context_st *ctx, dtls_alert_level_et level, dtls_alert_et description)
{
  uint8_t msg[] = { level, description };

  dtls_send(ctx, DTLS_CT_ALERT, msg, sizeof(msg));
  return 0;
}

int dtls_close(dtls_client_context_st *ctx)
{
  int res = -1;

  if (ctx) {
    res = dtls_send_alert(ctx, DTLS_ALERT_LEVEL_FATAL, DTLS_ALERT_CLOSE_NOTIFY);
    /* indicate tear down */
    ctx->e_state = DTLS_STATE_CLOSING;
  }
  return res;
}

static inline int dtls_send_ccs(dtls_client_context_st *ctx)
{
  uint8_t buf[1] = {1};

  return dtls_send(ctx, DTLS_CT_CHANGE_CIPHER_SPEC, buf, 1);
}


static int dtls_send_client_key_exchange(dtls_client_context_st *ctx)
{
  uint8_t buf[DTLS_CKXEC_LENGTH];
  uint8_t *p;
  dtls_handshake_parameters_st *handshake = &ctx->s_handshake_params;
  size_t sz_reslen;

  p = buf;

  switch (handshake->e_cipher) {

  case TLS_PSK_WITH_AES_128_CCM_8: {
    int len;
    sz_reslen = sizeof(buf) - sizeof(uint16_t);
    if (sz_reslen > sizeof(handshake->keyx.psk.identity)) {
      sz_reslen = sizeof(handshake->keyx.psk.identity);
    }
    len = CALL(ctx, get_psk_info, DTLS_PSK_IDENTITY,
               handshake->keyx.psk.identity, handshake->keyx.psk.id_length,
               buf + sizeof(uint16_t), sz_reslen);
    if (len < 0) {
      dtls_crit("no psk identity set in kx");
      return len;
    }

    if (len + sizeof(uint16_t) > DTLS_CKXEC_LENGTH) {
      memset(&handshake->keyx.psk, 0, sizeof(dtls_handshake_parameters_psk_st));
      dtls_warn("the psk identity is too long");
      return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
    }
    handshake->keyx.psk.id_length = (unsigned int)len;
    memcpy(handshake->keyx.psk.identity, p + sizeof(uint16_t), len);

    dtls_int_to_uint16(p, handshake->keyx.psk.id_length);
    p += sizeof(uint16_t);

    memcpy(p, handshake->keyx.psk.identity, handshake->keyx.psk.id_length);
    p += handshake->keyx.psk.id_length;

    break;
  }

  default:
    dtls_crit("cipher not supported");
    return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
  }

  assert(p - buf <= sizeof(buf));

  return dtls_send_handshake_msg(ctx, DTLS_HT_CLIENT_KEY_EXCHANGE, buf, p - buf);
}


static int dtls_send_finished(dtls_client_context_st *ctx, const uint8_t *label, size_t labellen)
{
  int length;
  uint8_t hash[DTLS_HMAC_MAX];
  uint8_t buf[DTLS_FIN_LENGTH];
  sha256_context_st hs_hash;
  uint8_t *p = buf;

  copy_hs_hash(ctx, &hs_hash);

  sha256Finalize(&hs_hash, hash);
  length = SHA256_DIGEST_LENGTH;

  dtls_prf(ctx->s_handshake_params.tmp.master_secret,
           DTLS_MASTER_SECRET_LENGTH,
           label, labellen,
           PRF_LABEL(finished), PRF_LABEL_SIZE(finished),
           hash, length,
           p, DTLS_FIN_LENGTH);

  dtls_debug_dump("server finished MAC", p, DTLS_FIN_LENGTH);

  p += DTLS_FIN_LENGTH;

  assert(p - buf <= sizeof(buf));

  return dtls_send_handshake_msg(ctx, DTLS_HT_FINISHED, buf, p - buf);
}

static int dtls_send_client_hello(dtls_client_context_st *ctx, uint8_t cookie[], size_t cookie_length)
{
  uint8_t buf[DTLS_CH_LENGTH_MAX];
  uint8_t *p = buf;
  uint8_t cipher_size;
  int psk;
  dtls_handshake_parameters_st *handshake;
  time_t now;

  handshake = &ctx->s_handshake_params;

  psk = is_psk_supported(ctx);

  cipher_size = 2 + (0) + ((psk) ? 2 : 0);

  if (cipher_size == 0) {
    dtls_crit("no cipher callbacks implemented");
  }

  dtls_int_to_uint16(p, DTLS_VERSION);
  p += sizeof(uint16_t);

  if (cookie_length > DTLS_COOKIE_LENGTH_MAX) {
    dtls_warn("the cookie is too long");
    return dtls_alert_fatal_create(DTLS_ALERT_HANDSHAKE_FAILURE);
  }

  if (cookie_length == 0) {
    /* Set client random: First 8 bytes are the client's Unix timestamp,
     * followed by 24 bytes of generate random data. */
    time(&now);
    memcpy(handshake->tmp.random.client, &now, sizeof(now));
    dtls_prng(handshake->tmp.random.client + sizeof(now),
         DTLS_RANDOM_LENGTH - sizeof(now));
  }
  /* we must use the same Client Random as for the previous request */
  memcpy(p, handshake->tmp.random.client, DTLS_RANDOM_LENGTH);
  p += DTLS_RANDOM_LENGTH;

  /* session id (length 0) */
  *p = 0;
  p += sizeof(uint8_t);

  /* cookie */
  *p = cookie_length;
  p += sizeof(uint8_t);
  if (cookie_length != 0) {
    memcpy(p, cookie, cookie_length);
    p += cookie_length;
  }

  /* add known cipher(s) */
  dtls_int_to_uint16(p, cipher_size - 2);
  p += sizeof(uint16_t);

  if (psk) {
    dtls_int_to_uint16(p, TLS_PSK_WITH_AES_128_CCM_8);
    p += sizeof(uint16_t);
  }

  /* compression method */
  *p = 1;
  p += sizeof(uint8_t);

  *p = TLS_COMPRESSION_NULL;
  p += sizeof(uint8_t);

  assert(p - buf <= sizeof(buf));

  if (cookie_length != 0)
    clear_hs_hash(ctx);

  return dtls_send_handshake_msg_hash(ctx, DTLS_HT_CLIENT_HELLO, buf, p - buf, cookie_length != 0);
}

static int check_server_hello(dtls_client_context_st *ctx, uint8_t *data, size_t data_length)
{
  dtls_handshake_parameters_st *handshake;

  handshake = &ctx->s_handshake_params;

  /* This function is called when we expect a ServerHello (i.e. we
   * have sent a ClientHello).  We might instead receive a HelloVerify
   * request containing a cookie. If so, we must repeat the
   * ClientHello with the given Cookie.
   */
  if (data_length < DTLS_HS_LENGTH + DTLS_HS_LENGTH)
    return dtls_alert_fatal_create(DTLS_ALERT_DECODE_ERROR);

  update_hs_hash(ctx, data, data_length);

  /* FIXME: check data_length before accessing fields */

  /* Get the server's random data and store selected cipher suite
   * and compression method (like dtls_update_parameters().
   * Then calculate master secret and wait for ServerHelloDone. When received,
   * send ClientKeyExchange (?) and ChangeCipherSpec + ClientFinished. */

  /* check server version */
  data += DTLS_HS_LENGTH;
  data_length -= DTLS_HS_LENGTH;

  if (dtls_uint16_to_int(data) != DTLS_VERSION) {
    dtls_alert("unknown DTLS version");
    return dtls_alert_fatal_create(DTLS_ALERT_PROTOCOL_VERSION);
  }

  data += sizeof(uint16_t);              /* skip version field */
  data_length -= sizeof(uint16_t);

  /* store server random data */
  memcpy(handshake->tmp.random.server, data, DTLS_RANDOM_LENGTH);
  /* skip server random */
  data += DTLS_RANDOM_LENGTH;
  data_length -= DTLS_RANDOM_LENGTH;

  if (data_length < (*data + sizeof(uint8_t)))
    goto error;
  data_length -= (*data + sizeof(uint8_t));
  data += (*data + sizeof(uint8_t));

  /* Check cipher suite. As we offer all we have, it is sufficient
   * to check if the cipher suite selected by the server is in our
   * list of known cipher suites. Subsets are not supported. */
  handshake->e_cipher = dtls_uint16_to_int(data);
  if (!known_cipher(ctx, handshake->e_cipher, 1)) {
    dtls_alert("unsupported cipher 0x%02x 0x%02x",
             data[0], data[1]);
    return dtls_alert_fatal_create(DTLS_ALERT_INSUFFICIENT_SECURITY);
  }
  data += sizeof(uint16_t);
  data_length -= sizeof(uint16_t);

  /* Check if NULL compression was selected. We do not know any other. */
  if (*data != TLS_COMPRESSION_NULL) {
    dtls_alert("unsupported compression method 0x%02x", data[0]);
    return dtls_alert_fatal_create(DTLS_ALERT_INSUFFICIENT_SECURITY);
  }
  data += sizeof(uint8_t);
  data_length -= sizeof(uint8_t);

  return 0;

error:
  return dtls_alert_fatal_create(DTLS_ALERT_DECODE_ERROR);
}

static int check_server_hello_verify_request(dtls_client_context_st *ctx, uint8_t *data, size_t data_length)
{
  dtls_hello_verify_st *hv;
  int res;

  if (data_length < DTLS_HS_LENGTH + DTLS_HV_LENGTH)
    return dtls_alert_fatal_create(DTLS_ALERT_DECODE_ERROR);

  hv = (dtls_hello_verify_st *)(data + DTLS_HS_LENGTH);

  res = dtls_send_client_hello(ctx, hv->au8_cookie, hv->u8_cookie_length);

  if (res < 0) {
    dtls_warn("cannot send ClientHello");
  }

  return res;
}


static int check_server_key_exchange_psk(dtls_client_context_st *ctx, uint8_t *data, size_t data_length)
{
  dtls_handshake_parameters_st *config = &ctx->s_handshake_params;
  uint16_t len;

  update_hs_hash(ctx, data, data_length);

  if (!(is_tls_psk_with_aes_128_ccm_8(config->e_cipher))) {
    dtls_alert("the packet cipher does not match the expected");
    return dtls_alert_fatal_create(DTLS_ALERT_DECODE_ERROR);
  }

  data += DTLS_HS_LENGTH;

  if (data_length < DTLS_HS_LENGTH + DTLS_SKEXECPSK_LENGTH_MIN) {
    dtls_alert("the packet length does not match the expected");
    return dtls_alert_fatal_create(DTLS_ALERT_DECODE_ERROR);
  }

  len = dtls_uint16_to_int(data);
  data += sizeof(uint16_t);

  if (len != data_length - DTLS_HS_LENGTH - sizeof(uint16_t)) {
    dtls_warn("the length of the server identity hint is worng");
    return dtls_alert_fatal_create(DTLS_ALERT_DECODE_ERROR);
  }

  if (len > DTLS_PSK_MAX_CLIENT_IDENTITY_LEN) {
    dtls_warn("please use a smaller server identity hint");
    return dtls_alert_fatal_create(DTLS_ALERT_INTERNAL_ERROR);
  }

  /* store the psk_identity_hint in config->keyx.psk for later use */
  config->keyx.psk.id_length = len;
  memcpy(config->keyx.psk.identity, data, len);
  return 0;
}

static inline void dtls_security_params_switch(dtls_client_context_st *ctx)
{
  if (ctx->ps_active_security_param == &ctx->as_security_params[0]) {
    ctx->ps_active_security_param = &ctx->as_security_params[1];
  } else {
    ctx->ps_active_security_param = &ctx->as_security_params[0];
  }
}

static int check_server_hellodone(dtls_client_context_st *ctx, uint8_t *data, size_t data_length)
{
  int res;

  dtls_handshake_parameters_st *handshake;

  handshake = &ctx->s_handshake_params;

  /* calculate master key, send CCS */

  update_hs_hash(ctx, data, data_length);

  /* send ClientKeyExchange */
  res = dtls_send_client_key_exchange(ctx);

  if (res < 0) {
    dtls_debug("cannot send KeyExchange message");
    return res;
  }


  res = calculate_key_block(ctx, handshake, DTLS_CLIENT);
  if (res < 0) {
    return res;
  }

  res = dtls_send_ccs(ctx);
  if (res < 0) {
    dtls_debug("cannot send CCS message");
    return res;
  }

  /* and switch cipher suite */
  dtls_security_params_switch(ctx);

  /* Client Finished */
  return dtls_send_finished(ctx, PRF_LABEL(client), PRF_LABEL_SIZE(client));
}

static inline dtls_security_parameters_st *dtls_security_params_epoch(dtls_client_context_st *ctx, uint16_t epoch)
{
  if (epoch == ctx->as_security_params[0].epoch) {
    return &ctx->as_security_params[0];
  } else if (epoch == ctx->as_security_params[1].epoch) {
    return &ctx->as_security_params[1];
  } else {
    return NULL;
  }
}

static int decrypt_verify(dtls_client_context_st *ctx, uint8_t *packet, size_t length, uint8_t **cleartext)
{
  dtls_record_header_st *header = DTLS_RECORD_HEADER(packet);
  dtls_security_parameters_st *security = dtls_security_params_epoch(ctx, dtls_get_epoch(header));
  int clen;

  *cleartext = (uint8_t *)packet + sizeof(dtls_record_header_st);
  clen = length - sizeof(dtls_record_header_st);

  if (!security) {
    dtls_alert("No security context for epoch: %i", dtls_get_epoch(header));
    return -1;
  }

  if (security->e_cipher == TLS_NULL_WITH_NULL_NULL) {
    /* no cipher suite selected */
    return clen;
  } else { /* TLS_PSK_WITH_AES_128_CCM_8 or TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8 */
    /**
     * length of additional_data for the AEAD cipher which consists of
     * seq_num(2+6) + type(1) + version(2) + length(2)
     */
#define A_DATA_LEN 13
    uint8_t nonce[DTLS_CCM_BLOCKSIZE];
    uint8_t A_DATA[A_DATA_LEN];

    if (clen < 16)                /* need at least IV and MAC */
      return -1;

    memset(nonce, 0, DTLS_CCM_BLOCKSIZE);
    memcpy(nonce, dtls_kb_remote_iv(security, DTLS_CLIENT), DTLS_IV_LENGTH);

    /* read epoch and seq_num from message */
    memcpy(nonce + DTLS_IV_LENGTH, *cleartext, 8);
    *cleartext += 8;
    clen -= 8;

    dtls_debug_dump("nonce", nonce, DTLS_CCM_BLOCKSIZE);
    dtls_debug_dump("ciphertext", *cleartext, clen);

    /* re-use N to create additional data according to RFC 5246, Section 6.2.3.3:
     *
     * additional_data = seq_num + TLSCompressed.type +
     *                   TLSCompressed.version + TLSCompressed.length;
     */
    memcpy(A_DATA, &DTLS_RECORD_HEADER(packet)->u16_epoch, 8); /* epoch and seq_num */
    memcpy(A_DATA + 8,  &DTLS_RECORD_HEADER(packet)->u8_content_type, 3); /* type and version */
    dtls_int_to_uint16(A_DATA + 11, clen - 8); /* length without nonce_explicit */

    clen = dtls_decrypt(&ctx->ps_active_security_param->s_cipher, *cleartext, clen, *cleartext, nonce,
                       dtls_kb_remote_write_key(security, DTLS_CLIENT),
                       DTLS_KEY_LENGTH, A_DATA, A_DATA_LEN);
    if (clen < 0)
      dtls_warn("decryption failed");
    else {
#ifndef NDEBUG
      //printf("decrypt_verify(): found %i bytes cleartext", clen);
#endif
      dtls_debug_dump("cleartext", *cleartext, clen);
    }
  }
  return clen;
}

int dtls_renegotiate(dtls_client_context_st *ctx)
{
  int err;

  if (DTLS_STATE_CONNECTED != ctx->e_state)
    return -1;

  dtls_handshake_init(&ctx->s_handshake_params);

  /* send ClientHello with empty Cookie */
  err = dtls_send_client_hello(ctx, NULL, 0);
  if (err < 0)
    dtls_warn("cannot send ClientHello");
  else
    ctx->e_state = DTLS_STATE_CLIENTHELLO;
  return err;
}

static int handle_handshake_msg(dtls_client_context_st *ctx, const dtls_peer_type role,
                                const dtls_state_et state, uint8_t *data, size_t data_length)
{

  int err = 0;

  /* The following switch construct handles the given message with
   * respect to the current internal state for this peer. In case of
   * error, it is left with return 0. */

  dtls_debug("handle handshake packet of type: %u", data[0]);
  switch (data[0]) {

  /************************************************************************
   * Client states
   ************************************************************************/
  case DTLS_HT_HELLO_VERIFY_REQUEST:

    if (state != DTLS_STATE_CLIENTHELLO) {
      return dtls_alert_fatal_create(DTLS_ALERT_UNEXPECTED_MESSAGE);
    }

    err = check_server_hello_verify_request(ctx, data, data_length);
    if (err < 0) {
      dtls_warn("error in check_server_hello_verify_request err: %i", err);
      return err;
    }

    break;
  case DTLS_HT_SERVER_HELLO:

    if (state != DTLS_STATE_CLIENTHELLO) {
      return dtls_alert_fatal_create(DTLS_ALERT_UNEXPECTED_MESSAGE);
    }

    err = check_server_hello(ctx, data, data_length);
    if (err < 0) {
      dtls_warn("error in check_server_hello err: %i", err);
      return err;
    }
    ctx->e_state = DTLS_STATE_WAIT_SERVERHELLODONE;
    /* update_hs_hash(peer, data, data_length); */

    break;


  case DTLS_HT_SERVER_KEY_EXCHANGE:
    if (is_tls_psk_with_aes_128_ccm_8(ctx->s_handshake_params.e_cipher)) {
      if (state != DTLS_STATE_WAIT_SERVERHELLODONE) {
        return dtls_alert_fatal_create(DTLS_ALERT_UNEXPECTED_MESSAGE);
      }
      err = check_server_key_exchange_psk(ctx, data, data_length);
    }

    if (err < 0) {
      dtls_warn("error in check_server_key_exchange err: %i", err);
      return err;
    }
    ctx->e_state = DTLS_STATE_WAIT_SERVERHELLODONE;
    /* update_hs_hash(peer, data, data_length); */

    break;

  case DTLS_HT_SERVER_HELLO_DONE:

    if (state != DTLS_STATE_WAIT_SERVERHELLODONE) {
      return dtls_alert_fatal_create(DTLS_ALERT_UNEXPECTED_MESSAGE);
    }

    err = check_server_hellodone(ctx, data, data_length);
    if (err < 0) {
      dtls_warn("error in check_server_hellodone err: %i", err);
      return err;
    }
    ctx->e_state = DTLS_STATE_WAIT_CHANGECIPHERSPEC;
    /* update_hs_hash(peer, data, data_length); */

    break;

  case DTLS_HT_CERTIFICATE_REQUEST:
    err = -1;
    dtls_warn("check_certificate_request not supported");
    break;

  case DTLS_HT_FINISHED:
    /* expect a Finished message from server */

    if (state != DTLS_STATE_WAIT_FINISHED) {
      return dtls_alert_fatal_create(DTLS_ALERT_UNEXPECTED_MESSAGE);
    }

    err = check_finished(ctx, data, data_length);
    if (err < 0) {
      dtls_warn("error in check_finished err: %i", err);
      return err;
    }
    if (role == DTLS_SERVER) {
      /* send ServerFinished */
      update_hs_hash(ctx, data, data_length);

      /* send change cipher spec message and switch to new configuration */
      err = dtls_send_ccs(ctx);
      if (err < 0) {
        dtls_warn("cannot send CCS message");
        return err;
      }

      dtls_security_params_switch(ctx);

      err = dtls_send_finished(ctx, PRF_LABEL(server), PRF_LABEL_SIZE(server));
      if (err < 0) {
        dtls_warn("sending server Finished failed");
        return err;
      }
    }

    dtls_debug("Handshake complete");
    ctx->e_state = DTLS_STATE_CONNECTED;

    /* return here to not increase the message receive counter */
    return err;

  /************************************************************************
   * Server states
   ************************************************************************/

  case DTLS_HT_CLIENT_KEY_EXCHANGE:
  case DTLS_HT_CLIENT_HELLO:
  case DTLS_HT_HELLO_REQUEST:

  default:
    dtls_crit("unhandled message %d", data[0]);
    return dtls_alert_fatal_create(DTLS_ALERT_UNEXPECTED_MESSAGE);
  }

  if (err >= 0) {
    ctx->s_handshake_params.hs_state.mseq_r++;
  }

  return err;
}

static int handle_handshake(dtls_client_context_st *ctx, const dtls_peer_type role,
                            const dtls_state_et state, uint8_t *data, size_t data_length)
{
  dtls_handshake_header_st *hs_header;
  int res;

  if (data_length < DTLS_HS_LENGTH) {
    dtls_warn("handshake message too short");
    return dtls_alert_fatal_create(DTLS_ALERT_DECODE_ERROR);
  }
  hs_header = DTLS_HANDSHAKE_HEADER(data);

  dtls_debug("received handshake packet of type: %u", hs_header->u8_msg_type);

  if (dtls_uint16_to_int(hs_header->u16_message_seq) < ctx->s_handshake_params.hs_state.mseq_r) {
    dtls_warn("The message sequence number is too small, expected %i, got: %i",
              ctx->s_handshake_params.hs_state.mseq_r, dtls_uint16_to_int(hs_header->u16_message_seq));
    return 0;
  } else if (dtls_uint16_to_int(hs_header->u16_message_seq) > ctx->s_handshake_params.hs_state.mseq_r) {

    /* TODO: only add packet that are not too new. */
    if (data_length > DTLS_MAX_BUF) {
      dtls_warn("the packet is too big to buffer for reoder");
      return 0;
    }

    dtls_info("Added packet for reordering");
    return 0;
  } else if (dtls_uint16_to_int(hs_header->u16_message_seq) == ctx->s_handshake_params.hs_state.mseq_r) {

    res = handle_handshake_msg(ctx, role, state, data, data_length);
    if (res < 0)
      return res;

    return res;
  }

  return 0;
}

static int handle_ccs(dtls_client_context_st *ctx, uint8_t *record_header, uint8_t *data, size_t data_length)
{
  /* A CCS message is handled after a KeyExchange message was
   * received from the client. When security parameters have been
   * updated successfully and a ChangeCipherSpec message was sent
   * by ourself, the security context is switched and the record
   * sequence number is reset. */

  if (!ctx || (DTLS_STATE_WAIT_CHANGECIPHERSPEC != ctx->e_state)) {
    dtls_warn("expected ChangeCipherSpec during handshake");
    return 0;
  }

  if (data_length < 1 || data[0] != 1)
    return dtls_alert_fatal_create(DTLS_ALERT_DECODE_ERROR);

  ctx->e_state = DTLS_STATE_WAIT_FINISHED;

  return 0;
}

/**
 * Handles incoming Alert messages. This function returns \c 1 if the
 * connection should be closed and the peer is to be invalidated.
 */
static int handle_alert(dtls_client_context_st *ctx, uint8_t *record_header, uint8_t *data, size_t data_length)
{
  int free_peer = 0;                /* indicates whether to free peer */

  if (data_length < 2)
    return dtls_alert_fatal_create(DTLS_ALERT_DECODE_ERROR);

  dtls_info("** Alert: level %d, description %d", data[0], data[1]);

  /* The peer object is invalidated for FATAL alerts and close
   * notifies. This is done in two steps.: First, remove the object
   * from our list of peers. After that, the event handler callback is
   * invoked with the still existing peer object. Finally, the storage
   * used by peer is released.
   */
  if (data[0] == DTLS_ALERT_LEVEL_FATAL || data[1] == DTLS_ALERT_CLOSE_NOTIFY) {
    //dtls_alert("%d invalidate peer", data[1]);

    free_peer = 1;

  }

  (void)CALL(ctx, event, (dtls_alert_level_et)data[0], (unsigned short)data[1]);
  switch (data[1]) {
  case DTLS_ALERT_CLOSE_NOTIFY:
    /* If state is DTLS_STATE_CLOSING, we have already sent a
     * close_notify so, do not send that again. */
    if (DTLS_STATE_CLOSING != ctx->e_state) {
      ctx->e_state = DTLS_STATE_CLOSING;
      dtls_send_alert(ctx, DTLS_ALERT_LEVEL_FATAL, DTLS_ALERT_CLOSE_NOTIFY);
    } else
      ctx->e_state = DTLS_STATE_CLOSED;
    break;
  default:
    ;
  }

  if ((DTLS_STATE_CLOSED != ctx->e_state) && (DTLS_STATE_CLOSING != ctx->e_state)) {
    dtls_close(ctx);
  }

  return free_peer;
}

static int dtls_alert_send_from_err(dtls_client_context_st *ctx, int err)
{
  int level;
  int desc;

  if (err < -(1 << 8) && err > -(3 << 8)) {
    level = ((-err) & 0xff00) >> 8;
    desc = (-err) & 0xff;
    if (ctx) {
      ctx->e_state = DTLS_STATE_CLOSING;
      return dtls_send_alert(ctx, level, desc);
    }
  } else if (err == -1) {
    if (ctx) {
      ctx->e_state = DTLS_STATE_CLOSING;
      return dtls_send_alert(ctx, DTLS_ALERT_LEVEL_FATAL, DTLS_ALERT_INTERNAL_ERROR);
    }
  }
  return -1;
}

/**
 * Handles incoming data as DTLS message from given peer.
 */
int dtls_handle_message(dtls_client_context_st *ctx, uint8_t *msg, int msglen)
{
  unsigned int rlen;                /* record length */
  uint8_t *data;                         /* (decrypted) payload */
  int data_length;                /* length of decrypted payload
                                   (without MAC and padding) */
  int err;

  while ((rlen = is_record(msg,msglen)) > 0) {
    dtls_peer_type role;
    dtls_state_et state;

    dtls_debug("got packet %d (%d bytes)", msg[0], rlen);
    if (ctx) {
      data_length = decrypt_verify(ctx, msg, rlen, &data);
      if (data_length < 0) {
        if (hs_attempt_with_existing_peer(msg, rlen, ctx)) {
          data = msg + DTLS_RH_LENGTH;
          data_length = rlen - DTLS_RH_LENGTH;
          state = DTLS_STATE_WAIT_CLIENTHELLO;
          role = DTLS_SERVER;
        } else {
          int err =  dtls_alert_fatal_create(DTLS_ALERT_DECRYPT_ERROR);
          dtls_info("decrypt_verify() failed");
          if (ctx->e_state < DTLS_STATE_CONNECTED) {
            dtls_alert_send_from_err(ctx, err);
            ctx->e_state = DTLS_STATE_CLOSED;
          }
          return err;
        }
      } else {
        role = DTLS_CLIENT;
        state = ctx->e_state;
      }
    } else {
      /* is_record() ensures that msg contains at least a record header */
      data = msg + DTLS_RH_LENGTH;
      data_length = rlen - DTLS_RH_LENGTH;
      state = DTLS_STATE_WAIT_CLIENTHELLO;
      role = DTLS_SERVER;
    }

    dtls_debug_hexdump("receive header", msg, sizeof(dtls_record_header_st));
    dtls_debug_hexdump("receive unencrypted", data, data_length);

    /* Handle received record according to the first byte of the
     * message, i.e. the subprotocol. We currently do not support
     * combining multiple fragments of one type into a single
     * record. */

    switch (msg[0]) {

    case DTLS_CT_CHANGE_CIPHER_SPEC:
      err = handle_ccs(ctx, msg, data, data_length);
      if (err < 0) {
        dtls_warn("error while handling ChangeCipherSpec message");
        dtls_alert_send_from_err(ctx, err);

        if ((DTLS_STATE_CLOSED != ctx->e_state) && (DTLS_STATE_CLOSING != ctx->e_state))
          dtls_close(ctx);

        return err;
      }
      break;

    case DTLS_CT_ALERT:
      err = handle_alert(ctx, msg, data, data_length);
      if (err < 0 || err == 1) {
         //dtls_warn("received alert, peer has been invalidated");
         dtls_warn("received alert");
         /* handle alert has invalidated peer */
         return err < 0 ?err:-1;
      }
      break;

    case DTLS_CT_HANDSHAKE:
      /* Handshake messages other than Finish must use the current
       * epoch, Finish has epoch + 1. */

      if (ctx) {
        uint16_t expected_epoch = ctx->ps_active_security_param->epoch;
        uint16_t msg_epoch =
          dtls_uint16_to_int(DTLS_RECORD_HEADER(msg)->u16_epoch);

        /* The new security parameters must be used for all messages
         * that are sent after the ChangeCipherSpec message. This
         * means that the client's Finished message uses epoch + 1
         * while the server is still in the old epoch.
         */
        if (role == DTLS_SERVER && state == DTLS_STATE_WAIT_FINISHED) {
          expected_epoch++;
        }

        if (expected_epoch != msg_epoch) {
          if (hs_attempt_with_existing_peer(msg, rlen, ctx)) {
            state = DTLS_STATE_WAIT_CLIENTHELLO;
            role = DTLS_SERVER;
          } else {
            dtls_warn("Wrong epoch, expected %i, got: %i",
                    expected_epoch, msg_epoch);
            break;
          }
        }
      }

      err = handle_handshake(ctx, role, state, data, data_length);
      if (err < 0) {
        dtls_warn("error while handling handshake packet");
        dtls_alert_send_from_err(ctx, err);
        return err;
      }
      if (ctx && (DTLS_STATE_CONNECTED == ctx->e_state)) {
        CALL(ctx, event, 0, DTLS_EVENT_CONNECTED);
      }
      break;

    case DTLS_CT_APPLICATION_DATA:
      dtls_info("** application data:");
      if (!ctx) {
        //dtls_warn("no peer available, send an alert");
        dtls_warn("no peer available");
        // TODO: should we send a alert here?
        return -1;
      }
      CALL(ctx, read, data, data_length);
      break;
    default:
      dtls_info("dropped unknown message of type %d",msg[0]);
    }

    /* advance msg by length of ciphertext */
    msg += rlen;
    msglen -= rlen;
  }

  return 0;
}

int dtls_client_init(dtls_client_context_st *p_ctx)
{
  time_t now;

  time(&now);
  srand((unsigned)now);

  dtls_security_init(&p_ctx->as_security_params[0]);
  dtls_security_init(&p_ctx->as_security_params[1]);
  p_ctx->ps_active_security_param = &p_ctx->as_security_params[0];

  return 0;
}

int dtls_connect(dtls_client_context_st *ctx)
{
  int res;

  /* send ClientHello with empty Cookie */
  dtls_handshake_init(&ctx->s_handshake_params);
  res = dtls_send_client_hello(ctx, NULL, 0);
  if (res < 0)
    dtls_warn("cannot send ClientHello");
  else
    ctx->e_state = DTLS_STATE_CLIENTHELLO;

  /* Invoke event callback to indicate connection attempt or
   * re-negotiation. */
  if (res > 0) {
    CALL(ctx, event, 0, DTLS_EVENT_CONNECT);
  } else if (res == 0) {
    CALL(ctx, event, 0, DTLS_EVENT_RENEGOTIATE);
  }

  return res;
}
