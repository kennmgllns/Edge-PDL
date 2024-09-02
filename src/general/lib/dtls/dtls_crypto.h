/**
 * @file dtls_crypto.h
 */

#ifndef _DTLS_CRYPTO_H_
#define _DTLS_CRYPTO_H_

#include <stdint.h>
#include <sys/types.h>

#include <mbedtls/aes.h> // will use "esp_aes"
#define aes_context_st        mbedtls_aes_context
#define aesSetKeyEncOnly      mbedtls_aes_setkey_enc
#define aesEncrypt            mbedtls_internal_aes_encrypt

#include <mbedtls/sha256.h> // will use "esp_sha256"
#define sha256_context_st     mbedtls_sha256_context
#define SHA256_DIGEST_LENGTH  (32)
#define sha256Init(ctx)       mbedtls_sha256_init(ctx); mbedtls_sha256_starts(ctx, 0)
#define sha256Update          mbedtls_sha256_update
#define sha256Finalize        mbedtls_sha256_finish

/* implementation of Counter Mode CBC-MAC, RFC 3610 */
#define DTLS_CCM_BLOCKSIZE                  16  // size of hmac blocks
#define DTLS_CCM_MAX                        16  // max number of bytes in digest
#define DTLS_CCM_NONCE_SIZE                 12  // size of nonce


/* TLS_PSK_WITH_AES_128_CCM_8 */
#define DTLS_KEY_LENGTH                     16  // AES-128
#define DTLS_BLK_LENGTH                     16  // AES-128
#define DTLS_MAC_LENGTH                     DTLS_HMAC_DIGEST_SIZE
#define DTLS_IV_LENGTH                      4   // length of nonce_explicit

/* Maximum size of the generated keyblock. */
#define MAX_KEYBLOCK_LENGTH                 ((2 * DTLS_KEY_LENGTH) + (2 * DTLS_IV_LENGTH))

/** Length of DTLS master_secret */
#define DTLS_MASTER_SECRET_LENGTH           48
#define DTLS_RANDOM_LENGTH                  32
#define DTLS_EC_KEY_SIZE                    32

/* maximal supported length of the psk client identity and psk server identity hint */
#define DTLS_PSK_MAX_CLIENT_IDENTITY_LEN    32
#define DTLS_PSK_MAX_KEY_LEN                DTLS_KEY_LENGTH

/** HMAC Keyed-Hash Message Authentication Code (HMAC) */
#define DTLS_HASH_CTX_SIZE                  sizeof(sha256_context_st)
#define DTLS_HMAC_BLOCKSIZE                 64	// size of hmac blocks
#define DTLS_HMAC_DIGEST_SIZE               32	// digest size (for SHA-256)
#define DTLS_HMAC_MAX                       64	// max number of bytes in digest


/* known cipher suites */
typedef enum
{
  TLS_NULL_WITH_NULL_NULL     = 0x0000,   // NULL cipher
  TLS_PSK_WITH_AES_128_CCM_8  = 0xC0A8,   // see RFC 6655
} dtls_cipher_et;

/* known compression suites */
typedef enum
{
  TLS_COMPRESSION_NULL = 0x0000           // NULL compression
} dtls_compression_et;

/* crypto context for TLS_PSK_WITH_AES_128_CCM_8 cipher suite */
typedef struct
{
  aes_context_st ccm_ctx;                   // AES-128 encryption context
} dtls_cipher_context_st;

typedef struct
{
  uint16_t  id_length;
  uint8_t   identity[DTLS_PSK_MAX_CLIENT_IDENTITY_LEN];
} dtls_handshake_parameters_psk_st;

typedef struct
{
  dtls_compression_et     e_compression;  // compression method
  dtls_cipher_et          e_cipher;       // cipher type
  dtls_cipher_context_st  s_cipher;       // ccm context
  uint16_t epoch;                         // counter for cipher state changes
  uint64_t rseq;                          // sequence number of last record sent
  uint8_t key_block[MAX_KEYBLOCK_LENGTH];
} dtls_security_parameters_st;

/** handshake protocol status */
typedef struct
{
  uint16_t            mseq_s;             // send handshake message sequence number counter
  uint16_t            mseq_r;             // received handshake message sequence number counter
//dtls_security_parameters_st  pending_config;
  sha256_context_st   hs_hash;            // temporary storage for the final handshake hash
} dtls_hs_state_st;


typedef struct
{
  union {
    struct random_t {
      uint8_t client[DTLS_RANDOM_LENGTH];             // client random gmt and bytes
      uint8_t server[DTLS_RANDOM_LENGTH];             // server random gmt and bytes
    } random;
    uint8_t master_secret[DTLS_MASTER_SECRET_LENGTH]; // the session's master secret
  } tmp;
  dtls_hs_state_st hs_state;                          // handshake protocol status

  dtls_compression_et e_compression;                  // compression method
  dtls_cipher_et      e_cipher;                       // cipher type
  unsigned int do_client_auth:1;
  union {
    dtls_handshake_parameters_psk_st psk;
  } keyx;
} dtls_handshake_parameters_st;

/* context for HMAC generation */
typedef struct {
  uint8_t pad[DTLS_HMAC_BLOCKSIZE];       // ipad and opad storage
  sha256_context_st    data;		                  // context for hash function
} dtls_hmac_context_st;

/* access to the key_block in the security parameters */
#define dtls_kb_client_write_key(Param)               ((Param)->key_block)
#define dtls_kb_server_write_key(Param)               (dtls_kb_client_write_key(Param) + DTLS_KEY_LENGTH)
#define dtls_kb_remote_write_key(Param, Role)         ((Role) == DTLS_SERVER ? dtls_kb_client_write_key(Param) : dtls_kb_server_write_key(Param))
#define dtls_kb_local_write_key(Param, Role)          ((Role) == DTLS_CLIENT ? dtls_kb_client_write_key(Param) : dtls_kb_server_write_key(Param))

#define dtls_kb_client_iv(Param)                      (dtls_kb_server_write_key(Param) + DTLS_KEY_LENGTH)
#define dtls_kb_server_iv(Param)                      (dtls_kb_client_iv(Param) + DTLS_IV_LENGTH)
#define dtls_kb_remote_iv(Param, Role)                ((Role) == DTLS_SERVER ? dtls_kb_client_iv(Param) : dtls_kb_server_iv(Param))
#define dtls_kb_local_iv(Param, Role)                 ((Role) == DTLS_CLIENT ? dtls_kb_client_iv(Param) : dtls_kb_server_iv(Param))


/* authenticates and encrypts a message using AES in CCM mode */
long int dtls_ccm_encrypt_message(aes_context_st *ctx, size_t M, size_t L, uint8_t nonce[DTLS_CCM_BLOCKSIZE],
                                  uint8_t *msg, size_t lm, const uint8_t *aad, size_t la);

long int dtls_ccm_decrypt_message(aes_context_st *ctx, size_t M, size_t L, uint8_t nonce[DTLS_CCM_BLOCKSIZE],
                                  uint8_t *msg, size_t lm, const uint8_t *aad, size_t la);


/* initializes an existing HMAC context */
void dtls_hmac_init(dtls_hmac_context_st *ctx, const uint8_t *key, size_t klen);


/* completes the HMAC generation and writes the result to the given output parameterresult */
int dtls_hmac_finalize(dtls_hmac_context_st *ctx, uint8_t *result);


/* expands the secret and key to a block of DTLS_HMAC_MAX size */
size_t dtls_p_hash(const uint8_t *key, size_t keylen, const uint8_t *label, size_t labellen,
                   const uint8_t *random1, size_t random1len, const uint8_t *random2, size_t random2len, uint8_t *buf, size_t buflen);

/* TLS PRF for DTLS_VERSION */
size_t dtls_prf(const uint8_t *key, size_t keylen, const uint8_t *label, size_t labellen,
                const uint8_t *random1, size_t random1len, const uint8_t *random2, size_t random2len, uint8_t *buf, size_t buflen);

/* encrypts the specified src of given length, writing the result to buf */
int dtls_encrypt(dtls_cipher_context_st *cipher, const uint8_t *src, size_t length, uint8_t *buf, uint8_t *nounce,
                 uint8_t *key, size_t keylen, const uint8_t *aad, size_t aad_length);

/* decrypts the given buffer src of given length, writing the result to buf. */
int dtls_decrypt(dtls_cipher_context_st *cipher, const uint8_t *src, size_t length, uint8_t *buf, uint8_t *nounce,
                 uint8_t *key, size_t keylen, const uint8_t *a_data, size_t a_data_length);

/* generates pre_master_sercet from given PSK */
int dtls_psk_pre_master_secret(uint8_t *key, size_t keylen, uint8_t *result, size_t result_len);

void dtls_handshake_init(dtls_handshake_parameters_st *hs);
void dtls_security_init(dtls_security_parameters_st *security);

#endif /* _DTLS_CRYPTO_H_ */
