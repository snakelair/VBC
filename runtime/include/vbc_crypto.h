#ifndef VBC_CRYPTO_H
#define VBC_CRYPTO_H

#include "vbc_runtime.h"

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* System.Convert & Hex/Base64 Helpers                                       */
/* ========================================================================= */

static const char vbc_hex_digits[] = "0123456789abcdef";

static inline vbc_string_t vbc_crypto_to_hex_string(vbc_stack_t* stack, const uint8_t* data, int64_t len) {
    if (!data || len <= 0) return vbc_str_empty();
    vbc_string_t res = vbc_str_alloc(stack, len * 2);
    char* dst = (char*)res.data;
    for (int64_t i = 0; i < len; i++) {
        dst[i * 2]     = vbc_hex_digits[(data[i] >> 4) & 0x0F];
        dst[i * 2 + 1] = vbc_hex_digits[data[i] & 0x0F];
    }
    return res;
}

static inline int vbc_hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static inline vbc_slice_t vbc_crypto_from_hex_string(vbc_stack_t* stack, vbc_string_t s) {
    int64_t byte_len = s.len / 2;
    vbc_slice_t slice = vbc_slice_create(stack, byte_len > 0 ? byte_len : 1, sizeof(uint8_t));
    if (!s.data || byte_len <= 0) return slice;
    for (int64_t i = 0; i < byte_len; i++) {
        int h1 = vbc_hex_val(s.data[i * 2]);
        int h2 = vbc_hex_val(s.data[i * 2 + 1]);
        if (h1 < 0 || h2 < 0) break;
        uint8_t byte_val = (uint8_t)((h1 << 4) | h2);
        vbc_slice_push(stack, &slice, &byte_val);
    }
    return slice;
}

static const char vbc_b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static inline vbc_string_t vbc_crypto_to_base64_string(vbc_stack_t* stack, const uint8_t* data, int64_t len) {
    if (!data || len <= 0) return vbc_str_empty();
    int64_t out_len = 4 * ((len + 2) / 3);
    vbc_string_t res = vbc_str_alloc(stack, out_len);
    char* out = (char*)res.data;
    int64_t i = 0, j = 0;
    while (i < len) {
        int64_t rem = len - i;
        uint32_t octet_a = (uint8_t)data[i++];
        uint32_t octet_b = (rem > 1) ? (uint8_t)data[i++] : 0;
        uint32_t octet_c = (rem > 2) ? (uint8_t)data[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out[j++] = vbc_b64_table[(triple >> 18) & 0x3F];
        out[j++] = vbc_b64_table[(triple >> 12) & 0x3F];
        out[j++] = (rem < 2) ? '=' : vbc_b64_table[(triple >> 6) & 0x3F];
        out[j++] = (rem < 3) ? '=' : vbc_b64_table[triple & 0x3F];
    }
    return res;
}

static inline vbc_slice_t vbc_crypto_from_base64_string(vbc_stack_t* stack, vbc_string_t s) {
    if (s.len <= 0 || !s.data) return vbc_slice_create(stack, 1, sizeof(uint8_t));
    int64_t est_len = (s.len * 3) / 4;
    vbc_slice_t slice = vbc_slice_create(stack, est_len > 0 ? est_len : 1, sizeof(uint8_t));

    int table[256];
    memset(table, -1, sizeof(table));
    for (int i = 0; i < 64; i++) table[(unsigned char)vbc_b64_table[i]] = i;

    uint32_t buf = 0;
    int bits = 0;
    for (int64_t i = 0; i < s.len; i++) {
        unsigned char c = (unsigned char)s.data[i];
        if (c == '=') break;
        if (table[c] == -1) continue;
        buf = (buf << 6) | (uint32_t)table[c];
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            uint8_t byte_val = (uint8_t)((buf >> bits) & 0xFF);
            vbc_slice_push(stack, &slice, &byte_val);
        }
    }
    return slice;
}

/* ========================================================================= */
/* System.BitConverter (Big-Endian & Little-Endian byte parser)               */
/* ========================================================================= */

static inline uint16_t vbc_bitconverter_to_uint16(const uint8_t* data, int64_t len, int64_t offset, bool is_big_endian) {
    if (!data || offset + 2 > len) return 0;
    if (is_big_endian) {
        return (uint16_t)(((uint16_t)data[offset] << 8) | (uint16_t)data[offset + 1]);
    } else {
        return (uint16_t)(((uint16_t)data[offset + 1] << 8) | (uint16_t)data[offset]);
    }
}

static inline uint32_t vbc_bitconverter_to_uint32(const uint8_t* data, int64_t len, int64_t offset, bool is_big_endian) {
    if (!data || offset + 4 > len) return 0;
    if (is_big_endian) {
        return ((uint32_t)data[offset] << 24) |
               ((uint32_t)data[offset + 1] << 16) |
               ((uint32_t)data[offset + 2] << 8) |
               ((uint32_t)data[offset + 3]);
    } else {
        return ((uint32_t)data[offset + 3] << 24) |
               ((uint32_t)data[offset + 2] << 16) |
               ((uint32_t)data[offset + 1] << 8) |
               ((uint32_t)data[offset]);
    }
}

/* ========================================================================= */
/* System.Security.Cryptography.RandomNumberGenerator                        */
/* ========================================================================= */

static inline vbc_slice_t vbc_crypto_get_random_bytes(vbc_stack_t* stack, int32_t count) {
    vbc_slice_t slice = vbc_slice_create(stack, count > 0 ? count : 1, sizeof(uint8_t));
    if (count <= 0) return slice;

    uint8_t* buf = (uint8_t*)vbc_stack_alloc(stack, count, 8);
#ifdef _WIN32
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        CryptGenRandom(hProv, (DWORD)count, buf);
        CryptReleaseContext(hProv, 0);
    }
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t rd = read(fd, buf, (size_t)count);
        (void)rd;
        close(fd);
    }
#endif
    slice.data = buf;
    slice.len = count;
    return slice;
}

/* ========================================================================= */
/* System.Security.Cryptography.SHA256 (RFC 6234 Native Implementation)      */
/* ========================================================================= */

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
} vbc_sha256_ctx;

#define VBC_ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define VBC_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define VBC_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define VBC_EP0(x) (VBC_ROTR(x, 2) ^ VBC_ROTR(x, 13) ^ VBC_ROTR(x, 22))
#define VBC_EP1(x) (VBC_ROTR(x, 6) ^ VBC_ROTR(x, 11) ^ VBC_ROTR(x, 25))
#define VBC_SIG0(x) (VBC_ROTR(x, 7) ^ VBC_ROTR(x, 18) ^ ((x) >> 3))
#define VBC_SIG1(x) (VBC_ROTR(x, 17) ^ VBC_ROTR(x, 19) ^ ((x) >> 10))

static const uint32_t vbc_sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static inline void vbc_sha256_transform(vbc_sha256_ctx* ctx, const uint8_t* data) {
    uint32_t a, b, c, d, e, f, g, h, t1, t2, m[64];
    for (int i = 0, j = 0; i < 16; ++i, j += 4)
        m[i] = ((uint32_t)data[j] << 24) | ((uint32_t)data[j+1] << 16) | ((uint32_t)data[j+2] << 8) | ((uint32_t)data[j+3]);
    for (int i = 16; i < 64; ++i)
        m[i] = VBC_SIG1(m[i-2]) + m[i-7] + VBC_SIG0(m[i-15]) + m[i-16];

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (int i = 0; i < 64; ++i) {
        t1 = h + VBC_EP1(e) + VBC_CH(e, f, g) + vbc_sha256_k[i] + m[i];
        t2 = VBC_EP0(a) + VBC_MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static inline void vbc_sha256_init(vbc_sha256_ctx* ctx) {
    ctx->count = 0;
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
}

static inline void vbc_sha256_update(vbc_sha256_ctx* ctx, const uint8_t* data, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        ctx->buffer[ctx->count % 64] = data[i];
        ctx->count++;
        if (ctx->count % 64 == 0)
            vbc_sha256_transform(ctx, ctx->buffer);
    }
}

static inline void vbc_sha256_final(vbc_sha256_ctx* ctx, uint8_t hash[32]) {
    uint64_t i = ctx->count % 64;
    ctx->buffer[i++] = 0x80;
    if (i > 56) {
        while (i < 64) ctx->buffer[i++] = 0x00;
        vbc_sha256_transform(ctx, ctx->buffer);
        memset(ctx->buffer, 0, 56);
    } else {
        while (i < 56) ctx->buffer[i++] = 0x00;
    }
    uint64_t total_bits = ctx->count * 8;
    for (int b = 7; b >= 0; b--) {
        ctx->buffer[56 + (7 - b)] = (uint8_t)(total_bits >> (b * 8));
    }
    vbc_sha256_transform(ctx, ctx->buffer);
    for (i = 0; i < 4; ++i) {
        hash[i]      = (uint8_t)((ctx->state[0] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 4]  = (uint8_t)((ctx->state[1] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 8]  = (uint8_t)((ctx->state[2] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 12] = (uint8_t)((ctx->state[3] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 16] = (uint8_t)((ctx->state[4] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 20] = (uint8_t)((ctx->state[5] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 24] = (uint8_t)((ctx->state[6] >> (24 - i * 8)) & 0x000000ff);
        hash[i + 28] = (uint8_t)((ctx->state[7] >> (24 - i * 8)) & 0x000000ff);
    }
}

static inline vbc_slice_t vbc_sha256_hash_data(vbc_stack_t* stack, const uint8_t* data, int64_t len) {
    vbc_slice_t slice = vbc_slice_create(stack, 32, sizeof(uint8_t));
    vbc_sha256_ctx ctx;
    vbc_sha256_init(&ctx);
    if (data && len > 0) {
        vbc_sha256_update(&ctx, data, (size_t)len);
    }
    uint8_t hash[32];
    vbc_sha256_final(&ctx, hash);
    for (int i = 0; i < 32; i++) {
        vbc_slice_push(stack, &slice, &hash[i]);
    }
    return slice;
}

static inline vbc_string_t vbc_sha256_hash_to_hex(vbc_stack_t* stack, const uint8_t* data, int64_t len) {
    vbc_sha256_ctx ctx;
    vbc_sha256_init(&ctx);
    if (data && len > 0) {
        vbc_sha256_update(&ctx, data, (size_t)len);
    }
    uint8_t hash[32];
    vbc_sha256_final(&ctx, hash);
    return vbc_crypto_to_hex_string(stack, hash, 32);
}

/* ========================================================================= */
/* System.Security.Cryptography.MD5 (RFC 1321 Native Implementation)         */
/* ========================================================================= */

typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];
} vbc_md5_ctx;

#define VBC_MD5_F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define VBC_MD5_G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define VBC_MD5_H(x, y, z) ((x) ^ (y) ^ (z))
#define VBC_MD5_I(x, y, z) ((y) ^ ((x) | (~z)))

#define VBC_MD5_STEP(f, a, b, c, d, x, t, s) \
    (a) += f((b), (c), (d)) + (x) + (t); \
    (a) = (((a) << (s)) | (((a) & 0xffffffff) >> (32 - (s)))); \
    (a) += (b);

static inline void vbc_md5_transform(uint32_t state[4], const uint8_t block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];
    for (int i = 0, j = 0; i < 16; ++i, j += 4)
        x[i] = ((uint32_t)block[j]) | (((uint32_t)block[j+1]) << 8) | (((uint32_t)block[j+2]) << 16) | (((uint32_t)block[j+3]) << 24);

    VBC_MD5_STEP(VBC_MD5_F, a, b, c, d, x[0],  0xd76aa478, 7);
    VBC_MD5_STEP(VBC_MD5_F, d, a, b, c, x[1],  0xe8c7b756, 12);
    VBC_MD5_STEP(VBC_MD5_F, c, d, a, b, x[2],  0x242070db, 17);
    VBC_MD5_STEP(VBC_MD5_F, b, c, d, a, x[3],  0xc1bdceee, 22);
    VBC_MD5_STEP(VBC_MD5_F, a, b, c, d, x[4],  0xf57c0faf, 7);
    VBC_MD5_STEP(VBC_MD5_F, d, a, b, c, x[5],  0x4787c62a, 12);
    VBC_MD5_STEP(VBC_MD5_F, c, d, a, b, x[6],  0xa8304613, 17);
    VBC_MD5_STEP(VBC_MD5_F, b, c, d, a, x[7],  0xfd469501, 22);
    VBC_MD5_STEP(VBC_MD5_F, a, b, c, d, x[8],  0x698098d8, 7);
    VBC_MD5_STEP(VBC_MD5_F, d, a, b, c, x[9],  0x8b44f7af, 12);
    VBC_MD5_STEP(VBC_MD5_F, c, d, a, b, x[10], 0xffff5bb1, 17);
    VBC_MD5_STEP(VBC_MD5_F, b, c, d, a, x[11], 0x895cd7be, 22);
    VBC_MD5_STEP(VBC_MD5_F, a, b, c, d, x[12], 0x6b901122, 7);
    VBC_MD5_STEP(VBC_MD5_F, d, a, b, c, x[13], 0xfd987193, 12);
    VBC_MD5_STEP(VBC_MD5_F, c, d, a, b, x[14], 0xa679438e, 17);
    VBC_MD5_STEP(VBC_MD5_F, b, c, d, a, x[15], 0x49b40821, 22);

    VBC_MD5_STEP(VBC_MD5_G, a, b, c, d, x[1],  0xf61e2562, 5);
    VBC_MD5_STEP(VBC_MD5_G, d, a, b, c, x[6],  0xc040b340, 9);
    VBC_MD5_STEP(VBC_MD5_G, c, d, a, b, x[11], 0x265e5a51, 14);
    VBC_MD5_STEP(VBC_MD5_G, b, c, d, a, x[0],  0xe9b6c7aa, 20);
    VBC_MD5_STEP(VBC_MD5_G, a, b, c, d, x[5],  0xd62f105d, 5);
    VBC_MD5_STEP(VBC_MD5_G, d, a, b, c, x[10], 0x02441453, 9);
    VBC_MD5_STEP(VBC_MD5_G, c, d, a, b, x[15], 0xd8a1e681, 14);
    VBC_MD5_STEP(VBC_MD5_G, b, c, d, a, x[4],  0xe7d3fbc8, 20);
    VBC_MD5_STEP(VBC_MD5_G, a, b, c, d, x[9],  0x21e1cde6, 5);
    VBC_MD5_STEP(VBC_MD5_G, d, a, b, c, x[14], 0xc33707d6, 9);
    VBC_MD5_STEP(VBC_MD5_G, c, d, a, b, x[3],  0xf4d50d87, 14);
    VBC_MD5_STEP(VBC_MD5_G, b, c, d, a, x[8],  0x455a14ed, 20);
    VBC_MD5_STEP(VBC_MD5_G, a, b, c, d, x[13], 0xa9e3e905, 5);
    VBC_MD5_STEP(VBC_MD5_G, d, a, b, c, x[2],  0xfcefa3f8, 9);
    VBC_MD5_STEP(VBC_MD5_G, c, d, a, b, x[7],  0x676f02d9, 14);
    VBC_MD5_STEP(VBC_MD5_G, b, c, d, a, x[12], 0x8d2a4c8a, 20);

    VBC_MD5_STEP(VBC_MD5_H, a, b, c, d, x[5],  0xfffa3942, 4);
    VBC_MD5_STEP(VBC_MD5_H, d, a, b, c, x[8],  0x8771f681, 11);
    VBC_MD5_STEP(VBC_MD5_H, c, d, a, b, x[11], 0x6d9d6122, 16);
    VBC_MD5_STEP(VBC_MD5_H, b, c, d, a, x[14], 0xfde5380c, 23);
    VBC_MD5_STEP(VBC_MD5_H, a, b, c, d, x[1],  0xa4beea44, 4);
    VBC_MD5_STEP(VBC_MD5_H, d, a, b, c, x[4],  0x4bdecfa9, 11);
    VBC_MD5_STEP(VBC_MD5_H, c, d, a, b, x[7],  0xf6bb4b60, 16);
    VBC_MD5_STEP(VBC_MD5_H, b, c, d, a, x[10], 0xbebfbc70, 23);
    VBC_MD5_STEP(VBC_MD5_H, a, b, c, d, x[13], 0x289b7ec6, 4);
    VBC_MD5_STEP(VBC_MD5_H, d, a, b, c, x[0],  0xeaa127fa, 11);
    VBC_MD5_STEP(VBC_MD5_H, c, d, a, b, x[3],  0xd4ef3085, 16);
    VBC_MD5_STEP(VBC_MD5_H, b, c, d, a, x[6],  0x04881d05, 23);
    VBC_MD5_STEP(VBC_MD5_H, a, b, c, d, x[9],  0xd9d4d039, 4);
    VBC_MD5_STEP(VBC_MD5_H, d, a, b, c, x[12], 0xe6db99e5, 11);
    VBC_MD5_STEP(VBC_MD5_H, c, d, a, b, x[15], 0x1fa27cf8, 16);
    VBC_MD5_STEP(VBC_MD5_H, b, c, d, a, x[2],  0xc4ac5665, 23);

    VBC_MD5_STEP(VBC_MD5_I, a, b, c, d, x[0],  0xf4292244, 6);
    VBC_MD5_STEP(VBC_MD5_I, d, a, b, c, x[7],  0x432aff97, 10);
    VBC_MD5_STEP(VBC_MD5_I, c, d, a, b, x[14], 0xab9423a7, 15);
    VBC_MD5_STEP(VBC_MD5_I, b, c, d, a, x[5],  0xfc93a039, 21);
    VBC_MD5_STEP(VBC_MD5_I, a, b, c, d, x[12], 0x655b59c3, 6);
    VBC_MD5_STEP(VBC_MD5_I, d, a, b, c, x[3],  0x8f0ccc92, 10);
    VBC_MD5_STEP(VBC_MD5_I, c, d, a, b, x[10], 0xffeff47d, 15);
    VBC_MD5_STEP(VBC_MD5_I, b, c, d, a, x[1],  0x85845dd1, 21);
    VBC_MD5_STEP(VBC_MD5_I, a, b, c, d, x[8],  0x6fa87e4f, 6);
    VBC_MD5_STEP(VBC_MD5_I, d, a, b, c, x[15], 0xfe2ce6e0, 10);
    VBC_MD5_STEP(VBC_MD5_I, c, d, a, b, x[6],  0xa3014314, 15);
    VBC_MD5_STEP(VBC_MD5_I, b, c, d, a, x[13], 0x4e0811a1, 21);
    VBC_MD5_STEP(VBC_MD5_I, a, b, c, d, x[4],  0xf7537e82, 6);
    VBC_MD5_STEP(VBC_MD5_I, d, a, b, c, x[11], 0xbd3af235, 10);
    VBC_MD5_STEP(VBC_MD5_I, c, d, a, b, x[2],  0x2ad7d2bb, 15);
    VBC_MD5_STEP(VBC_MD5_I, b, c, d, a, x[9],  0xeb86d391, 21);

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

static inline void vbc_md5_init(vbc_md5_ctx* ctx) {
    ctx->count[0] = ctx->count[1] = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
}

static inline void vbc_md5_update(vbc_md5_ctx* ctx, const uint8_t* data, size_t len) {
    uint32_t in_len = (uint32_t)len;
    uint32_t i, index, part_len;
    index = (ctx->count[0] >> 3) & 0x3F;
    if ((ctx->count[0] += (in_len << 3)) < (in_len << 3))
        ctx->count[1]++;
    ctx->count[1] += (in_len >> 29);
    part_len = 64 - index;

    if (in_len >= part_len) {
        memcpy(&ctx->buffer[index], data, part_len);
        vbc_md5_transform(ctx->state, ctx->buffer);
        for (i = part_len; i + 63 < in_len; i += 64)
            vbc_md5_transform(ctx->state, &data[i]);
        index = 0;
    } else {
        i = 0;
    }
    memcpy(&ctx->buffer[index], &data[i], in_len - i);
}

static inline void vbc_md5_final(vbc_md5_ctx* ctx, uint8_t digest[16]) {
    static const uint8_t padding[64] = { 0x80 };
    uint8_t bits[8];
    for (int i = 0; i < 4; i++) bits[i] = (uint8_t)((ctx->count[0] >> (i * 8)) & 0xFF);
    for (int i = 0; i < 4; i++) bits[i + 4] = (uint8_t)((ctx->count[1] >> (i * 8)) & 0xFF);

    uint32_t index = (ctx->count[0] >> 3) & 0x3F;
    uint32_t pad_len = (index < 56) ? (56 - index) : (120 - index);
    vbc_md5_update(ctx, padding, pad_len);
    vbc_md5_update(ctx, bits, 8);

    for (int i = 0; i < 4; i++) {
        digest[i]      = (uint8_t)((ctx->state[0] >> (i * 8)) & 0xFF);
        digest[i + 4]  = (uint8_t)((ctx->state[1] >> (i * 8)) & 0xFF);
        digest[i + 8]  = (uint8_t)((ctx->state[2] >> (i * 8)) & 0xFF);
        digest[i + 12] = (uint8_t)((ctx->state[3] >> (i * 8)) & 0xFF);
    }
}

static inline vbc_slice_t vbc_md5_hash_data(vbc_stack_t* stack, const uint8_t* data, int64_t len) {
    vbc_slice_t slice = vbc_slice_create(stack, 16, sizeof(uint8_t));
    vbc_md5_ctx ctx;
    vbc_md5_init(&ctx);
    if (data && len > 0) {
        vbc_md5_update(&ctx, data, (size_t)len);
    }
    uint8_t digest[16];
    vbc_md5_final(&ctx, digest);
    for (int i = 0; i < 16; i++) {
        vbc_slice_push(stack, &slice, &digest[i]);
    }
    return slice;
}

static inline vbc_string_t vbc_md5_hash_to_hex(vbc_stack_t* stack, const uint8_t* data, int64_t len) {
    vbc_md5_ctx ctx;
    vbc_md5_init(&ctx);
    if (data && len > 0) {
        vbc_md5_update(&ctx, data, (size_t)len);
    }
    uint8_t digest[16];
    vbc_md5_final(&ctx, digest);
    return vbc_crypto_to_hex_string(stack, digest, 16);
}

#ifdef __cplusplus
}
#endif

#endif /* VBC_CRYPTO_H */
