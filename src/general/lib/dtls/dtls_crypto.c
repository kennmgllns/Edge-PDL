/**
 * @file dtls_crypto.c
 */


#include <stdio.h>
#include <string.h>

#include "global_defs.h"
#include "dtls_client.h"
#include "dtls_crypto.h"


#if 0
  #define dtls_debug    LOGD
#else
  #define dtls_debug(...)
#endif
#define dtls_warn       LOGW


#ifndef max
  #define max(A,B) ((A) < (B) ? (B) : (A))
#endif

#define CCM_FLAGS(A,M,L) (((A > 0) << 6) | (((M - 2)/2) << 3) | (L - 1))

#define MASK_L(_L) ((1 << 8 * _L) - 1)

#define SET_COUNTER(A,L,cnt,C) {                                           \
    int i;                                                                 \
    memset((A) + DTLS_CCM_BLOCKSIZE - (L), 0, (L));                        \
    (C) = (cnt) & MASK_L(L);                                               \
    for (i = DTLS_CCM_BLOCKSIZE - 1; (C) && (i > (L)); --i, (C) >>= 8)     \
      (A)[i] |= (C) & 0xFF;                                                \
  }


#define HMAC_UPDATE_SEED(Context,Seed,Length)     do { if (NULL != Seed) sha256Update(&(Context.data), (Seed), (Length)); } while (0)

static inline void block0(size_t M,       /* number of auth bytes */
                          size_t L,       /* number of bytes to encode message length */
                          size_t la,      /* l(a) octets additional authenticated data */
                          size_t lm,      /* l(m) message length */
                          uint8_t nonce[DTLS_CCM_BLOCKSIZE],
                          uint8_t *result)
{
  int i;

  result[0] = CCM_FLAGS(la, M, L);

  /* copy the nonce */
  memcpy(result + 1, nonce, DTLS_CCM_BLOCKSIZE - L - 1);

  for (i=0; i < L; i++) {
    result[15-i] = lm & 0xff;
    lm >>= 8;
  }
}

/* XORs \p n bytes byte-by-byte starting at \p y to the memory area starting at \p x. */
static inline void memxor(uint8_t *x, const uint8_t *y, size_t n)
{
  while(n--) {
    *x ^= *y;
    x++; y++;
  }
}

/* creates the CBC-MAC for the additional authentication data that is sent in cleartext */
static void add_auth_data(aes_context_st *ctx, const uint8_t *msg, size_t la,
                          uint8_t B[DTLS_CCM_BLOCKSIZE], uint8_t X[DTLS_CCM_BLOCKSIZE])
{
  size_t i,j;

  aesEncrypt(ctx, B, X);

  memset(B, 0, DTLS_CCM_BLOCKSIZE);

  if (!la) {
    return;
  }

    if (la < 0xFF00) {                /* 2^16 - 2^8 */
      j = 2;
      B[0] = ((uint16_t)la >> 8) & 0xff;
      B[1] = (uint16_t)la & 0xff;
  } else if (la <= UINT32_MAX) {
      j = 6;
      B[0] = 0xff;
      B[1] = 0xfe;
      B[2] = ((uint32_t)la >> 24) & 0xff;
      B[3] = ((uint32_t)la >> 16) & 0xff;
      B[4] = ((uint32_t)la >> 8) & 0xff;
      B[5] = (uint32_t)la & 0xff;
    } else {
      j = 10;
      B[0] = 0xff;
      B[1] = 0xff;
      B[2] = ((uint64_t)la >> 56) & 0xff;
      B[3] = ((uint64_t)la >> 48) & 0xff;
      B[4] = ((uint64_t)la >> 40) & 0xff;
      B[5] = ((uint64_t)la >> 32) & 0xff;
      B[6] = ((uint64_t)la >> 24) & 0xff;
      B[7] = ((uint64_t)la >> 16) & 0xff;
      B[8] = ((uint64_t)la >> 8) & 0xff;
      B[9] = (uint64_t)la & 0xff;
    }

    i = DTLS_CCM_BLOCKSIZE - j;
    if (i > la) {
      i = la;
    }
    memcpy(B + j, msg, i);
    la -= i;
    msg += i;

    memxor(B, X, DTLS_CCM_BLOCKSIZE);

  aesEncrypt(ctx, B, X);

  while (la > DTLS_CCM_BLOCKSIZE) {
    for (i = 0; i < DTLS_CCM_BLOCKSIZE; ++i)
      B[i] = X[i] ^ *msg++;
    la -= DTLS_CCM_BLOCKSIZE;

    aesEncrypt(ctx, B, X);
  }

  if (la) {
    memset(B, 0, DTLS_CCM_BLOCKSIZE);
    memcpy(B, msg, la);
    memxor(B, X, DTLS_CCM_BLOCKSIZE);

    aesEncrypt(ctx, B, X);
  }
}

static inline void encrypt(aes_context_st *ctx, size_t L, unsigned long counter,
                          uint8_t *msg, size_t len, uint8_t A[DTLS_CCM_BLOCKSIZE], uint8_t S[DTLS_CCM_BLOCKSIZE])
{
  static unsigned long counter_tmp;

  SET_COUNTER(A, L, counter, counter_tmp);
  aesEncrypt(ctx, A, S);
  memxor(msg, S, len);
}

static inline void mac(aes_context_st *ctx, uint8_t *msg, size_t len,
                       uint8_t B[DTLS_CCM_BLOCKSIZE], uint8_t X[DTLS_CCM_BLOCKSIZE])
{
  size_t i;

  for (i = 0; i < len; ++i)
    B[i] = X[i] ^ msg[i];

  aesEncrypt(ctx, B, X);

}

long int dtls_ccm_encrypt_message(aes_context_st *ctx, size_t M, size_t L, uint8_t nonce[DTLS_CCM_BLOCKSIZE],
                                  uint8_t *msg, size_t lm, const uint8_t *aad, size_t la)
{
  size_t i, len;
  unsigned long counter_tmp;
  unsigned long counter = 1; /* \bug does not work correctly on ia32 when
                                     lm >= 2^16 */
  uint8_t A[DTLS_CCM_BLOCKSIZE]; /* A_i blocks for encryption input */
  uint8_t B[DTLS_CCM_BLOCKSIZE]; /* B_i blocks for CBC-MAC input */
  uint8_t S[DTLS_CCM_BLOCKSIZE]; /* S_i = encrypted A_i blocks */
  uint8_t X[DTLS_CCM_BLOCKSIZE]; /* X_i = encrypted B_i blocks */

  len = lm;                        /* save original length */
  /* create the initial authentication block B0 */
  block0(M, L, la, lm, nonce, B);
  add_auth_data(ctx, aad, la, B, X);

  /* initialize block template */
  A[0] = L-1;

  /* copy the nonce */
  memcpy(A + 1, nonce, DTLS_CCM_BLOCKSIZE - L - 1);

  while (lm >= DTLS_CCM_BLOCKSIZE) {
    /* calculate MAC */
    mac(ctx, msg, DTLS_CCM_BLOCKSIZE, B, X);

    /* encrypt */
    encrypt(ctx, L, counter, msg, DTLS_CCM_BLOCKSIZE, A, S);

    /* update local pointers */
    lm -= DTLS_CCM_BLOCKSIZE;
    msg += DTLS_CCM_BLOCKSIZE;
    counter++;
  }

  if (lm) {
    /* Calculate MAC. The remainder of B must be padded with zeroes, so
     * B is constructed to contain X ^ msg for the first lm bytes (done in
     * mac() and X ^ 0 for the remaining DTLS_CCM_BLOCKSIZE - lm bytes
     * (i.e., we can use memcpy() here).
     */
    memcpy(B + lm, X + lm, DTLS_CCM_BLOCKSIZE - lm);
    mac(ctx, msg, lm, B, X);

    /* encrypt */
    encrypt(ctx, L, counter, msg, lm, A, S);

    /* update local pointers */
    msg += lm;
  }

  /* calculate S_0 */
  SET_COUNTER(A, L, 0, counter_tmp);
  aesEncrypt(ctx, A, S);

  for (i = 0; i < M; ++i)
    *msg++ = X[i] ^ S[i];

  return len + M;
}

long int dtls_ccm_decrypt_message(aes_context_st *ctx, size_t M, size_t L, uint8_t nonce[DTLS_CCM_BLOCKSIZE],
                                  uint8_t *msg, size_t lm, const uint8_t *aad, size_t la)
{

  size_t len;
  unsigned long counter_tmp;
  unsigned long counter = 1; /* \bug does not work correctly on ia32 when
                                     lm >= 2^16 */
  uint8_t A[DTLS_CCM_BLOCKSIZE]; /* A_i blocks for encryption input */
  uint8_t B[DTLS_CCM_BLOCKSIZE]; /* B_i blocks for CBC-MAC input */
  uint8_t S[DTLS_CCM_BLOCKSIZE]; /* S_i = encrypted A_i blocks */
  uint8_t X[DTLS_CCM_BLOCKSIZE]; /* X_i = encrypted B_i blocks */

  if (lm < M)
    goto error;

  len = lm;              /* save original length */
  lm -= M;              /* detract MAC size*/

  /* create the initial authentication block B0 */
  block0(M, L, la, lm, nonce, B);
  add_auth_data(ctx, aad, la, B, X);

  /* initialize block template */
  A[0] = L-1;

  /* copy the nonce */
  memcpy(A + 1, nonce, DTLS_CCM_BLOCKSIZE - L - 1);

  while (lm >= DTLS_CCM_BLOCKSIZE) {
    /* decrypt */
    encrypt(ctx, L, counter, msg, DTLS_CCM_BLOCKSIZE, A, S);

    /* calculate MAC */
    mac(ctx, msg, DTLS_CCM_BLOCKSIZE, B, X);

    /* update local pointers */
    lm -= DTLS_CCM_BLOCKSIZE;
    msg += DTLS_CCM_BLOCKSIZE;
    counter++;
  }

  if (lm) {
    /* decrypt */
    encrypt(ctx, L, counter, msg, lm, A, S);

    /* Calculate MAC. Note that msg ends in the MAC so we must
     * construct B to contain X ^ msg for the first lm bytes (done in
     * mac() and X ^ 0 for the remaining DTLS_CCM_BLOCKSIZE - lm bytes
     * (i.e., we can use memcpy() here).
     */
    memcpy(B + lm, X + lm, DTLS_CCM_BLOCKSIZE - lm);
    mac(ctx, msg, lm, B, X);

    /* update local pointers */
    msg += lm;
  }

  /* calculate S_0 */
  SET_COUNTER(A, L, 0, counter_tmp);
  aesEncrypt(ctx, A, S);

  memxor(msg, S, M);

  /* return length if MAC is valid, otherwise continue with error handling */
  if (0 == memcmp(X, msg, M))
    return len - M;

 error:
  return -1;
}

void dtls_hmac_init(dtls_hmac_context_st *ctx, const uint8_t *key, size_t klen)
{
  int i;

  memset(ctx, 0, sizeof(dtls_hmac_context_st));

  if (klen > DTLS_HMAC_BLOCKSIZE) {
    sha256Init(&ctx->data);
    sha256Update(&ctx->data, key, klen);
    sha256Finalize(&ctx->data, ctx->pad);
  } else
    memcpy(ctx->pad, key, klen);

  /* create ipad: */
  for (i=0; i < DTLS_HMAC_BLOCKSIZE; ++i)
    ctx->pad[i] ^= 0x36;

  sha256Init(&ctx->data);
  sha256Update(&ctx->data, ctx->pad, DTLS_HMAC_BLOCKSIZE);

  /* create opad by xor-ing pad[i] with 0x36 ^ 0x5C: */
  for (i=0; i < DTLS_HMAC_BLOCKSIZE; ++i)
    ctx->pad[i] ^= 0x6A;
}

int dtls_hmac_finalize(dtls_hmac_context_st *ctx, uint8_t *result)
{
  uint8_t buf[DTLS_HMAC_DIGEST_SIZE];

  sha256Finalize(&ctx->data, buf);

  sha256Init(&ctx->data);
  sha256Update(&ctx->data, ctx->pad, DTLS_HMAC_BLOCKSIZE);
  sha256Update(&ctx->data, buf, SHA256_DIGEST_LENGTH);

  sha256Finalize(&ctx->data, result);

  return SHA256_DIGEST_LENGTH;
}


void dtls_handshake_init(dtls_handshake_parameters_st *hs)
{
  memset(hs, 0, sizeof(dtls_handshake_parameters_st));

  /* initialize the handshake hash wrt. the hard-coded DTLS version */
  dtls_debug("DTLSv12: initialize HASH_SHA256");
  /* TLS 1.2:  PRF(secret, label, seed) = P_<hash>(secret, label + seed) */
  sha256Init(&hs->hs_state.hs_hash);
}

void dtls_security_init(dtls_security_parameters_st *security)
{
  memset(security, 0, sizeof(*security));
  security->e_cipher = TLS_NULL_WITH_NULL_NULL;
  security->e_compression = TLS_COMPRESSION_NULL;
}

size_t dtls_p_hash(const uint8_t *key, size_t keylen, const uint8_t *label, size_t labellen,
                   const uint8_t *random1, size_t random1len, const uint8_t *random2, size_t random2len,
                   uint8_t *buf, size_t buflen)
{
  dtls_hmac_context_st hmac_a;
  dtls_hmac_context_st hmac_p;

  uint8_t A[DTLS_HMAC_DIGEST_SIZE];
  uint8_t tmp[DTLS_HMAC_DIGEST_SIZE];
  size_t dlen;                        /* digest length */
  size_t len = 0;                        /* result length */

  dtls_hmac_init(&hmac_a, key, keylen);

  /* calculate A(1) from A(0) == seed */
  HMAC_UPDATE_SEED(hmac_a, label, labellen);
  HMAC_UPDATE_SEED(hmac_a, random1, random1len);
  HMAC_UPDATE_SEED(hmac_a, random2, random2len);

  dlen = dtls_hmac_finalize(&hmac_a, A);

  dtls_hmac_init(&hmac_p, key, keylen);

  while (len + dlen < buflen) {

    /* FIXME: rewrite loop to avoid superflous call to dtls_hmac_init() */
    dtls_hmac_init(&hmac_p, key, keylen);
    HMAC_UPDATE_SEED(hmac_p, A, dlen);

    HMAC_UPDATE_SEED(hmac_p, label, labellen);
    HMAC_UPDATE_SEED(hmac_p, random1, random1len);
    HMAC_UPDATE_SEED(hmac_p, random2, random2len);

    len += dtls_hmac_finalize(&hmac_p, tmp);
    memcpy(buf, tmp, dlen);
    buf += dlen;

    /* calculate A(i+1) */
    dtls_hmac_init(&hmac_a, key, keylen);
    HMAC_UPDATE_SEED(hmac_a, A, dlen);
    dtls_hmac_finalize(&hmac_a, A);
  }

  dtls_hmac_init(&hmac_p, key, keylen);
  HMAC_UPDATE_SEED(hmac_p, A, dlen);

  HMAC_UPDATE_SEED(hmac_p, label, labellen);
  HMAC_UPDATE_SEED(hmac_p, random1, random1len);
  HMAC_UPDATE_SEED(hmac_p, random2, random2len);

  dtls_hmac_finalize(&hmac_p, tmp);
  memcpy(buf, tmp, buflen - len);

  return buflen;
}

size_t  dtls_prf(const uint8_t *key, size_t keylen, const uint8_t *label, size_t labellen,
                 const uint8_t *random1, size_t random1len, const uint8_t *random2, size_t random2len,
                 uint8_t *buf, size_t buflen)
{

  /* Clear the result buffer */
  memset(buf, 0, buflen);
  return dtls_p_hash(key, keylen, label, labellen, random1, random1len, random2, random2len, buf, buflen);
}

static size_t dtls_ccm_encrypt(aes_context_st *ccm_ctx, const uint8_t *src, size_t srclen,
                               uint8_t *buf,  uint8_t *nounce, const uint8_t *aad, size_t la)
{
  long int len;

  len = dtls_ccm_encrypt_message(ccm_ctx, 8 /* M */, max(2, 15 - DTLS_CCM_NONCE_SIZE), nounce, buf, srclen, aad, la);
  return len;
}

static size_t dtls_ccm_decrypt(aes_context_st *ccm_ctx, const uint8_t *src,
                               size_t srclen, uint8_t *buf, uint8_t *nounce, const uint8_t *aad, size_t la)
{
  long int len;

  len = dtls_ccm_decrypt_message(ccm_ctx, 8 /* M */, max(2, 15 - DTLS_CCM_NONCE_SIZE), nounce, buf, srclen, aad, la);
  return len;
}

int dtls_psk_pre_master_secret(uint8_t *key, size_t keylen, uint8_t *result, size_t result_len)
{
  uint8_t *p = result;

  if (result_len < (2 * (sizeof(uint16_t) + keylen))) {
    return -1;
  }

  *p++ = ((uint16_t)keylen >> 8) & 0xff;
  *p++ = (uint16_t)keylen & 0xff;

  memset(p, 0, keylen);
  p += keylen;

  memcpy(p, result, sizeof(uint16_t));
  p += sizeof(uint16_t);

  memcpy(p, key, keylen);

  return 2 * (sizeof(uint16_t) + keylen);
}

int dtls_encrypt(dtls_cipher_context_st *cipher, const uint8_t *src, size_t length,
                 uint8_t *buf, uint8_t *nounce, uint8_t *key, size_t keylen, const uint8_t *aad, size_t la)
{
  int ret;

  ret = aesSetKeyEncOnly(&cipher->ccm_ctx, key, 8 * keylen);
  if (ret < 0) {
    /* cleanup everything in case the key has the wrong size */
    dtls_warn("cannot set rijndael key");
    goto error;
  }

  if (src != buf)
    memmove(buf, src, length);
  ret = dtls_ccm_encrypt(&cipher->ccm_ctx, src, length, buf, nounce, aad, la);

error:
  return ret;
}

int dtls_decrypt(dtls_cipher_context_st *cipher, const uint8_t *src, size_t length, uint8_t *buf, uint8_t *nounce,
                 uint8_t *key, size_t keylen, const uint8_t *aad, size_t la)
{
  int ret;

  ret = aesSetKeyEncOnly(&cipher->ccm_ctx, key, 8 * keylen);
  if (ret < 0) {
    /* cleanup everything in case the key has the wrong size */
    dtls_warn("cannot set rijndael key");
    goto error;
  }

  if (src != buf)
    memmove(buf, src, length);
  ret = dtls_ccm_decrypt(&cipher->ccm_ctx, src, length, buf, nounce, aad, la);

error:
  return ret;
}
