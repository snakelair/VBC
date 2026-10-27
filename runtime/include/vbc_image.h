#ifndef VBC_IMAGE_H
#define VBC_IMAGE_H

#include "vbc_runtime.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t width;
    int32_t height;
    int32_t channels;
    uint8_t* pixels;
} vbc_bitmap_t;

/* ========================================================================= */
/* Image Loading                                                             */
/* ========================================================================= */

static inline vbc_bitmap_t vbc_image_load_from_bytes(vbc_stack_t* stack, const uint8_t* data, int64_t len) {
    vbc_bitmap_t bmp = {0, 0, 0, NULL};
    if (!data || len <= 0) return bmp;

    int w = 0, h = 0, ch = 0;
    // Request 3 channels (RGB) or 4 (RGBA)
    unsigned char* raw = stbi_load_from_memory(data, (int)len, &w, &h, &ch, 3);
    if (!raw) return bmp;

    size_t sz = (size_t)(w * h * 3);
    uint8_t* arena_pixels = (uint8_t*)vbc_stack_alloc(stack, sz, 8);
    if (arena_pixels) {
        memcpy(arena_pixels, raw, sz);
        bmp.width = w;
        bmp.height = h;
        bmp.channels = 3;
        bmp.pixels = arena_pixels;
    }
    stbi_image_free(raw);
    return bmp;
}

static inline vbc_bitmap_t vbc_image_load_from_file(vbc_stack_t* stack, vbc_string_t path) {
    vbc_bitmap_t bmp = {0, 0, 0, NULL};
    char path_buf[512];
    int64_t plen = path.len < 511 ? path.len : 511;
    memcpy(path_buf, path.data, (size_t)plen);
    path_buf[plen] = '\0';

    FILE* f = fopen(path_buf, "rb");
    if (!f) return bmp;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return bmp; }

    uint8_t* file_buf = (uint8_t*)malloc((size_t)sz);
    if (!file_buf) { fclose(f); return bmp; }
    size_t rd = fread(file_buf, 1, (size_t)sz, f);
    fclose(f);

    if (rd > 0) {
        bmp = vbc_image_load_from_bytes(stack, file_buf, (int64_t)rd);
    }
    free(file_buf);
    return bmp;
}

/* ========================================================================= */
/* High-Speed Integer Bilinear Scaling (Router CPU Friendly)                 */
/* ========================================================================= */

static inline vbc_bitmap_t vbc_image_resize(vbc_stack_t* stack, const vbc_bitmap_t* src, int32_t target_w, int32_t target_h) {
    vbc_bitmap_t dst = {0, 0, 0, NULL};
    if (!src || !src->pixels || src->width <= 0 || src->height <= 0 || target_w <= 0 || target_h <= 0) return dst;

    size_t sz = (size_t)(target_w * target_h * src->channels);
    uint8_t* dst_pixels = (uint8_t*)vbc_stack_alloc(stack, sz, 8);
    if (!dst_pixels) return dst;

    int ch = src->channels;
    int src_w = src->width;
    int src_h = src->height;

    // Fixed-point 16.16 coordinates
    int x_ratio = ((src_w - 1) << 16) / target_w;
    int y_ratio = ((src_h - 1) << 16) / target_h;

    for (int y = 0; y < target_h; y++) {
        int sy = (y * y_ratio) >> 16;
        int y_diff = (y * y_ratio) & 0xFFFF;
        int y_diff_inv = 0x10000 - y_diff;
        int row_a = sy * src_w * ch;
        int row_b = (sy + 1 < src_h ? sy + 1 : sy) * src_w * ch;
        int dst_row = y * target_w * ch;

        for (int x = 0; x < target_w; x++) {
            int sx = (x * x_ratio) >> 16;
            int x_diff = (x * x_ratio) & 0xFFFF;
            int x_diff_inv = 0x10000 - x_diff;

            int sx1 = (sx + 1 < src_w ? sx + 1 : sx);

            for (int c = 0; c < ch; c++) {
                int a = src->pixels[row_a + sx * ch + c];
                int b = src->pixels[row_a + sx1 * ch + c];
                int d = src->pixels[row_b + sx * ch + c];
                int e = src->pixels[row_b + sx1 * ch + c];

                int val = ((a * x_diff_inv + b * x_diff) >> 16) * y_diff_inv +
                          ((d * x_diff_inv + e * x_diff) >> 16) * y_diff;

                dst_pixels[dst_row + x * ch + c] = (uint8_t)(val >> 16);
            }
        }
    }

    dst.width = target_w;
    dst.height = target_h;
    dst.channels = ch;
    dst.pixels = dst_pixels;
    return dst;
}

/* ========================================================================= */
/* EXIF Orientation & Rotate/Flip Transform (Tags 1..8)                      */
/* ========================================================================= */

static inline void vbc_image_rotate_flip(vbc_bitmap_t* bmp, int32_t rotate_flip_type) {
    if (!bmp || !bmp->pixels || bmp->width <= 0 || bmp->height <= 0) return;
    int w = bmp->width;
    int h = bmp->height;
    int ch = bmp->channels;

    // Type 1: Normal (no-op)
    if (rotate_flip_type <= 1) return;

    size_t sz = (size_t)(w * h * ch);
    uint8_t* temp = (uint8_t*)malloc(sz);
    if (!temp) return;
    memcpy(temp, bmp->pixels, sz);

    switch (rotate_flip_type) {
        case 2: // Flip Horizontal
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    memcpy(&bmp->pixels[(y * w + (w - 1 - x)) * ch], &temp[(y * w + x) * ch], (size_t)ch);
                }
            }
            break;
        case 3: // Rotate 180
            for (int i = 0; i < w * h; i++) {
                memcpy(&bmp->pixels[(w * h - 1 - i) * ch], &temp[i * ch], (size_t)ch);
            }
            break;
        case 4: // Flip Vertical
            for (int y = 0; y < h; y++) {
                memcpy(&bmp->pixels[((h - 1 - y) * w) * ch], &temp[(y * w) * ch], (size_t)(w * ch));
            }
            break;
        case 5: // Transpose (Flip Horizontal & Rotate 270)
            bmp->width = h;
            bmp->height = w;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    memcpy(&bmp->pixels[(x * h + y) * ch], &temp[(y * w + x) * ch], (size_t)ch);
                }
            }
            break;
        case 6: // Rotate 90 CW
            bmp->width = h;
            bmp->height = w;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    memcpy(&bmp->pixels[(x * h + (h - 1 - y)) * ch], &temp[(y * w + x) * ch], (size_t)ch);
                }
            }
            break;
        case 7: // Transverse
            bmp->width = h;
            bmp->height = w;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    memcpy(&bmp->pixels[((w - 1 - x) * h + (h - 1 - y)) * ch], &temp[(y * w + x) * ch], (size_t)ch);
                }
            }
            break;
        case 8: // Rotate 270 CW (90 CCW)
            bmp->width = h;
            bmp->height = w;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    memcpy(&bmp->pixels[((w - 1 - x) * h + y) * ch], &temp[(y * w + x) * ch], (size_t)ch);
                }
            }
            break;
        default:
            break;
    }
    free(temp);
}

/* ========================================================================= */
/* JPEG Output Writing                                                       */
/* ========================================================================= */

typedef struct {
    vbc_stack_t* stack;
    vbc_slice_t slice;
} vbc_stbi_mem_context;

static void vbc_stbi_write_func(void* context, void* data, int size) {
    vbc_stbi_mem_context* ctx = (vbc_stbi_mem_context*)context;
    uint8_t* bytes = (uint8_t*)data;
    for (int i = 0; i < size; i++) {
        vbc_slice_push(ctx->stack, &ctx->slice, &bytes[i]);
    }
}

static inline vbc_slice_t vbc_image_to_jpeg_bytes(vbc_stack_t* stack, const vbc_bitmap_t* bmp, int32_t quality) {
    vbc_stbi_mem_context ctx;
    ctx.stack = stack;
    ctx.slice = vbc_slice_create(stack, 4096, sizeof(uint8_t));

    if (!bmp || !bmp->pixels || bmp->width <= 0 || bmp->height <= 0) return ctx.slice;
    if (quality <= 0) quality = 80;
    if (quality > 100) quality = 100;

    stbi_write_jpg_to_func(vbc_stbi_write_func, &ctx, bmp->width, bmp->height, bmp->channels, bmp->pixels, quality);
    return ctx.slice;
}

static inline bool vbc_image_save_jpeg(const vbc_bitmap_t* bmp, vbc_string_t path, int32_t quality) {
    if (!bmp || !bmp->pixels || bmp->width <= 0 || bmp->height <= 0) return false;
    if (quality <= 0) quality = 80;
    char path_buf[512];
    int64_t plen = path.len < 511 ? path.len : 511;
    memcpy(path_buf, path.data, (size_t)plen);
    path_buf[plen] = '\0';

    return stbi_write_jpg(path_buf, bmp->width, bmp->height, bmp->channels, bmp->pixels, quality) != 0;
}

#ifdef __cplusplus
}
#endif

#endif /* VBC_IMAGE_H */
