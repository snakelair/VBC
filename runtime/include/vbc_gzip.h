#ifndef VBC_GZIP_H
#define VBC_GZIP_H

#include "vbc_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* CRC32 (IEEE 802.3)                                                        */
/* ========================================================================= */

static inline uint32_t vbc_crc32(uint32_t crc, const uint8_t* buf, size_t len) {
    crc = ~crc;
    while (len--) {
        crc ^= *buf++;
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(int32_t)(crc & 1)));
        }
    }
    return ~crc;
}

/* ========================================================================= */
/* Tiny Inflate (RFC 1951 / RFC 1952)                                        */
/* ========================================================================= */

typedef struct {
    const uint8_t* source;
    size_t source_len;
    size_t source_pos;
    uint32_t bit_buf;
    int bit_count;
} vbc_tinf_data;

static inline uint32_t vbc_tinf_get_bits(vbc_tinf_data* d, int num) {
    while (d->bit_count < num) {
        if (d->source_pos >= d->source_len) return 0;
        d->bit_buf |= ((uint32_t)d->source[d->source_pos++]) << d->bit_count;
        d->bit_count += 8;
    }
    uint32_t res = d->bit_buf & ((1U << num) - 1);
    d->bit_buf >>= num;
    d->bit_count -= num;
    return res;
}

typedef struct {
    uint16_t count[16];
    uint16_t symbol[288];
} vbc_tinf_tree;

static inline void vbc_tinf_build_tree(vbc_tinf_tree* t, const uint8_t* lengths, int num) {
    uint16_t offsets[16];
    memset(t->count, 0, sizeof(t->count));
    for (int i = 0; i < num; i++) t->count[lengths[i]]++;
    t->count[0] = 0;
    offsets[1] = 0;
    for (int i = 1; i < 15; i++) offsets[i + 1] = offsets[i] + t->count[i];
    for (int i = 0; i < num; i++) {
        if (lengths[i] != 0) t->symbol[offsets[lengths[i]]++] = (uint16_t)i;
    }
}

static inline int vbc_tinf_decode_symbol(vbc_tinf_data* d, const vbc_tinf_tree* t) {
    int cur = 0, sum = 0;
    for (int len = 1; len <= 15; len++) {
        cur |= (int)vbc_tinf_get_bits(d, 1);
        sum += t->count[len];
        cur -= t->count[len];
        if (cur < 0) return t->symbol[sum + cur];
        cur <<= 1;
    }
    return -1;
}

static const uint16_t vbc_length_base[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258
};
static const uint8_t vbc_length_extra[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const uint16_t vbc_dist_base[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
};
static const uint8_t vbc_dist_extra[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
};
static const uint8_t vbc_clc_order[19] = {
    16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
};

static inline void vbc_tinf_build_fixed_trees(vbc_tinf_tree* lt, vbc_tinf_tree* dt) {
    uint8_t lengths[288];
    for (int i = 0; i <= 143; i++) lengths[i] = 8;
    for (int i = 144; i <= 255; i++) lengths[i] = 9;
    for (int i = 256; i <= 279; i++) lengths[i] = 7;
    for (int i = 280; i <= 287; i++) lengths[i] = 8;
    vbc_tinf_build_tree(lt, lengths, 288);
    for (int i = 0; i < 30; i++) lengths[i] = 5;
    vbc_tinf_build_tree(dt, lengths, 30);
}

static inline int vbc_tinf_inflate_block(vbc_tinf_data* d, const vbc_tinf_tree* lt, const vbc_tinf_tree* dt, vbc_slice_t* out, vbc_stack_t* stack) {
    while (1) {
        int sym = vbc_tinf_decode_symbol(d, lt);
        if (sym < 0) return -1;
        if (sym < 256) {
            uint8_t b = (uint8_t)sym;
            vbc_slice_push(stack, out, &b);
        } else if (sym == 256) {
            return 0; // End of block
        } else {
            sym -= 257;
            if (sym >= 29) return -1;
            int length = vbc_length_base[sym] + (int)vbc_tinf_get_bits(d, vbc_length_extra[sym]);
            int dist_sym = vbc_tinf_decode_symbol(d, dt);
            if (dist_sym < 0 || dist_sym >= 30) return -1;
            int dist = vbc_dist_base[dist_sym] + (int)vbc_tinf_get_bits(d, vbc_dist_extra[dist_sym]);
            if (dist > out->len) return -1;

            int64_t start_offset = out->len - dist;
            for (int i = 0; i < length; i++) {
                uint8_t b = ((uint8_t*)out->data)[start_offset + i];
                vbc_slice_push(stack, out, &b);
            }
        }
    }
}

/* Decompress Gzip Stream */
static inline vbc_slice_t vbc_gzip_decompress(vbc_stack_t* stack, const uint8_t* data, int64_t len) {
    if (!data || len < 10) return vbc_slice_create(stack, 1, sizeof(uint8_t));

    // Check GZIP ID1, ID2, CM
    if (data[0] != 0x1F || data[1] != 0x8B || data[2] != 0x08) {
        return vbc_slice_create(stack, 1, sizeof(uint8_t));
    }
    uint8_t flg = data[3];
    int64_t pos = 10;

    if (flg & 4) { // FEXTRA
        if (pos + 2 > len) return vbc_slice_create(stack, 1, sizeof(uint8_t));
        uint16_t xlen = (uint16_t)(data[pos] | (data[pos+1] << 8));
        pos += 2 + xlen;
    }
    if (flg & 8) { // FNAME
        while (pos < len && data[pos] != 0) pos++;
        pos++;
    }
    if (flg & 16) { // FCOMMENT
        while (pos < len && data[pos] != 0) pos++;
        pos++;
    }
    if (flg & 2) { // FHCRC
        pos += 2;
    }
    if (pos >= len - 8) return vbc_slice_create(stack, 1, sizeof(uint8_t));

    vbc_tinf_data d;
    d.source = data;
    d.source_len = (size_t)(len - 8); // Skip trailer
    d.source_pos = (size_t)pos;
    d.bit_buf = 0;
    d.bit_count = 0;

    vbc_slice_t out = vbc_slice_create(stack, 4096, sizeof(uint8_t));
    int bfinal = 0;

    while (!bfinal) {
        bfinal = (int)vbc_tinf_get_bits(&d, 1);
        int btype = (int)vbc_tinf_get_bits(&d, 2);

        if (btype == 0) {
            // Uncompressed block
            d.bit_buf = 0;
            d.bit_count = 0;
            if (d.source_pos + 4 > d.source_len) break;
            uint16_t blen = (uint16_t)(d.source[d.source_pos] | (d.source[d.source_pos+1] << 8));
            d.source_pos += 4; // Skip len and nlen
            if (d.source_pos + blen > d.source_len) break;
            for (uint16_t i = 0; i < blen; i++) {
                uint8_t b = d.source[d.source_pos++];
                vbc_slice_push(stack, &out, &b);
            }
        } else if (btype == 1) {
            // Fixed Huffman
            vbc_tinf_tree lt, dt;
            vbc_tinf_build_fixed_trees(&lt, &dt);
            if (vbc_tinf_inflate_block(&d, &lt, &dt, &out, stack) < 0) break;
        } else if (btype == 2) {
            // Dynamic Huffman
            int hlit = (int)vbc_tinf_get_bits(&d, 5) + 257;
            int hdist = (int)vbc_tinf_get_bits(&d, 5) + 1;
            int hclen = (int)vbc_tinf_get_bits(&d, 4) + 4;

            uint8_t cl_lens[19] = {0};
            for (int i = 0; i < hclen; i++) cl_lens[vbc_clc_order[i]] = (uint8_t)vbc_tinf_get_bits(&d, 3);
            vbc_tinf_tree ct;
            vbc_tinf_build_tree(&ct, cl_lens, 19);

            uint8_t lengths[288 + 32] = {0};
            int num_total = hlit + hdist;
            int cur_idx = 0;
            while (cur_idx < num_total) {
                int sym = vbc_tinf_decode_symbol(&d, &ct);
                if (sym < 16) {
                    lengths[cur_idx++] = (uint8_t)sym;
                } else if (sym == 16) {
                    int rep = (int)vbc_tinf_get_bits(&d, 2) + 3;
                    uint8_t prev = cur_idx > 0 ? lengths[cur_idx - 1] : 0;
                    while (rep--) lengths[cur_idx++] = prev;
                } else if (sym == 17) {
                    int rep = (int)vbc_tinf_get_bits(&d, 3) + 3;
                    while (rep--) lengths[cur_idx++] = 0;
                } else if (sym == 18) {
                    int rep = (int)vbc_tinf_get_bits(&d, 7) + 11;
                    while (rep--) lengths[cur_idx++] = 0;
                } else {
                    break;
                }
            }
            vbc_tinf_tree lt, dt;
            vbc_tinf_build_tree(&lt, lengths, hlit);
            vbc_tinf_build_tree(&dt, lengths + hlit, hdist);
            if (vbc_tinf_inflate_block(&d, &lt, &dt, &out, stack) < 0) break;
        } else {
            break; // Error
        }
    }

    return out;
}

/* ========================================================================= */
/* Fast Gzip Compressor (RFC 1952 / Deflate Fixed Huffman)                   */
/* ========================================================================= */

typedef struct {
    uint8_t* out;
    size_t out_cap;
    size_t out_len;
    uint32_t bit_buf;
    int bit_count;
} vbc_deflate_writer;

static inline void vbc_dw_put_bits(vbc_deflate_writer* w, uint32_t bits, int num) {
    w->bit_buf |= (bits << w->bit_count);
    w->bit_count += num;
    while (w->bit_count >= 8) {
        if (w->out_len < w->out_cap) w->out[w->out_len++] = (uint8_t)(w->bit_buf & 0xFF);
        w->bit_buf >>= 8;
        w->bit_count -= 8;
    }
}

static inline void vbc_dw_flush_bits(vbc_deflate_writer* w) {
    if (w->bit_count > 0) {
        if (w->out_len < w->out_cap) w->out[w->out_len++] = (uint8_t)(w->bit_buf & 0xFF);
        w->bit_buf = 0;
        w->bit_count = 0;
    }
}

static inline void vbc_write_fixed_huffman_literal(vbc_deflate_writer* w, int lit) {
    // RFC 1951 fixed Huffman codes:
    // 0 - 143: 8 bits, code = lit + 0x30
    // 144 - 255: 9 bits, code = lit - 144 + 0x190
    // 256 - 279: 7 bits, code = lit - 256
    // 280 - 287: 8 bits, code = lit - 280 + 0xC0
    if (lit <= 143) {
        uint32_t code = (uint32_t)(lit + 0x30);
        uint32_t rev = 0;
        for (int i = 0; i < 8; i++) rev = (rev << 1) | ((code >> i) & 1);
        vbc_dw_put_bits(w, rev, 8);
    } else if (lit <= 255) {
        uint32_t code = (uint32_t)(lit - 144 + 0x190);
        uint32_t rev = 0;
        for (int i = 0; i < 9; i++) rev = (rev << 1) | ((code >> i) & 1);
        vbc_dw_put_bits(w, rev, 9);
    } else if (lit <= 279) {
        uint32_t code = (uint32_t)(lit - 256);
        uint32_t rev = 0;
        for (int i = 0; i < 7; i++) rev = (rev << 1) | ((code >> i) & 1);
        vbc_dw_put_bits(w, rev, 7);
    } else {
        uint32_t code = (uint32_t)(lit - 280 + 0xC0);
        uint32_t rev = 0;
        for (int i = 0; i < 8; i++) rev = (rev << 1) | ((code >> i) & 1);
        vbc_dw_put_bits(w, rev, 8);
    }
}

static inline vbc_slice_t vbc_gzip_compress(vbc_stack_t* stack, const uint8_t* data, int64_t len) {
    // Upper bound for gzip overhead: header (10) + trailer (8) + raw uncompressed data
    size_t max_out = (size_t)(len + 64 + (len / 10));
    uint8_t* out_buf = (uint8_t*)vbc_stack_alloc(stack, max_out, 8);
    vbc_slice_t slice = vbc_slice_create(stack, 1, sizeof(uint8_t));
    if (!out_buf) return slice;

    vbc_deflate_writer w;
    w.out = out_buf;
    w.out_cap = max_out;
    w.out_len = 0;
    w.bit_buf = 0;
    w.bit_count = 0;

    // Gzip Header
    w.out[w.out_len++] = 0x1F;
    w.out[w.out_len++] = 0x8B;
    w.out[w.out_len++] = 0x08; // CM = deflate
    w.out[w.out_len++] = 0x00; // FLG
    w.out[w.out_len++] = 0x00; w.out[w.out_len++] = 0x00; w.out[w.out_len++] = 0x00; w.out[w.out_len++] = 0x00; // MTIME
    w.out[w.out_len++] = 0x00; // XFL
    w.out[w.out_len++] = 0xFF; // OS = Unknown

    // Single Deflate Block (BFINAL=1, BTYPE=01 Fixed Huffman)
    vbc_dw_put_bits(&w, 1, 1); // BFINAL = 1
    vbc_dw_put_bits(&w, 1, 2); // BTYPE = 01 (Fixed Huffman)

    for (int64_t i = 0; i < len; i++) {
        vbc_write_fixed_huffman_literal(&w, data[i]);
    }
    vbc_write_fixed_huffman_literal(&w, 256); // End of block

    vbc_dw_flush_bits(&w);

    // Gzip Trailer: CRC32 + ISIZE (4 bytes each)
    uint32_t crc = vbc_crc32(0, data, (size_t)len);
    w.out[w.out_len++] = (uint8_t)(crc & 0xFF);
    w.out[w.out_len++] = (uint8_t)((crc >> 8) & 0xFF);
    w.out[w.out_len++] = (uint8_t)((crc >> 16) & 0xFF);
    w.out[w.out_len++] = (uint8_t)((crc >> 24) & 0xFF);

    uint32_t isize = (uint32_t)len;
    w.out[w.out_len++] = (uint8_t)(isize & 0xFF);
    w.out[w.out_len++] = (uint8_t)((isize >> 8) & 0xFF);
    w.out[w.out_len++] = (uint8_t)((isize >> 16) & 0xFF);
    w.out[w.out_len++] = (uint8_t)((isize >> 24) & 0xFF);

    slice.data = out_buf;
    slice.len = w.out_len;
    return slice;
}

#ifdef __cplusplus
}
#endif

#endif /* VBC_GZIP_H */
