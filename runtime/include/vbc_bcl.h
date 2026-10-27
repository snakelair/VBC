#ifndef VBC_BCL_H
#define VBC_BCL_H

#include "vbc_runtime.h"
#include <math.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* System.Math & System.MathF                                                */
/* ========================================================================= */

#define VBC_MATH_PI  3.14159265358979323846
#define VBC_MATH_E   2.71828182845904523536
#define VBC_MATH_TAU 6.28318530717958647692

static inline int64_t vbc_math_abs_i(int64_t v) { return v < 0 ? -v : v; }
static inline double vbc_math_abs_d(double v) { return fabs(v); }
static inline int64_t vbc_math_min_i(int64_t a, int64_t b) { return a < b ? a : b; }
static inline double vbc_math_min_d(double a, double b) { return a < b ? a : b; }
static inline int64_t vbc_math_max_i(int64_t a, int64_t b) { return a > b ? a : b; }
static inline double vbc_math_max_d(double a, double b) { return a > b ? a : b; }
static inline int64_t vbc_math_clamp_i(int64_t val, int64_t min, int64_t max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}
static inline double vbc_math_clamp_d(double val, double min, double max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}
static inline double vbc_math_floor(double v) { return floor(v); }
static inline double vbc_math_ceiling(double v) { return ceil(v); }
static inline double vbc_math_round(double v) { return round(v); }
static inline double vbc_math_truncate(double v) { return trunc(v); }
static inline int32_t vbc_math_sign(double v) { return (v > 0.0) ? 1 : ((v < 0.0) ? -1 : 0); }
static inline double vbc_math_sqrt(double v) { return sqrt(v); }
static inline double vbc_math_pow(double x, double y) { return pow(x, y); }
static inline double vbc_math_exp(double v) { return exp(v); }
static inline double vbc_math_log(double v) { return log(v); }
static inline double vbc_math_log10(double v) { return log10(v); }
static inline double vbc_math_sin(double v) { return sin(v); }
static inline double vbc_math_cos(double v) { return cos(v); }
static inline double vbc_math_tan(double v) { return tan(v); }
static inline double vbc_math_asin(double v) { return asin(v); }
static inline double vbc_math_acos(double v) { return acos(v); }
static inline double vbc_math_atan(double v) { return atan(v); }
static inline double vbc_math_atan2(double y, double x) { return atan2(y, x); }

/* ========================================================================= */
/* System.String Extended BCL Methods                                        */
/* ========================================================================= */

static inline bool vbc_str_starts_with(vbc_string_t s, vbc_string_t prefix) {
    if (prefix.len == 0) return true;
    if (s.len < prefix.len || !s.data || !prefix.data) return false;
    return memcmp(s.data, prefix.data, (size_t)prefix.len) == 0;
}

static inline bool vbc_str_ends_with(vbc_string_t s, vbc_string_t suffix) {
    if (suffix.len == 0) return true;
    if (s.len < suffix.len || !s.data || !suffix.data) return false;
    return memcmp(s.data + (s.len - suffix.len), suffix.data, (size_t)suffix.len) == 0;
}

static inline bool vbc_str_contains(vbc_string_t s, vbc_string_t target) {
    return vbc_str_index_of(s, target, 0) >= 0;
}

static inline int64_t vbc_str_last_index_of(vbc_string_t s, vbc_string_t target) {
    if (target.len == 0) return s.len;
    if (s.len < target.len || !s.data || !target.data) return -1;
    for (int64_t i = s.len - target.len; i >= 0; i--) {
        if (memcmp(s.data + i, target.data, (size_t)target.len) == 0) return i;
    }
    return -1;
}

static inline vbc_string_t vbc_str_trim(vbc_stack_t* stack, vbc_string_t s) {
    if (!s.data || s.len == 0) return vbc_str_empty();
    int64_t start = 0;
    while (start < s.len && (unsigned char)s.data[start] <= ' ') start++;
    int64_t end = s.len - 1;
    while (end >= start && (unsigned char)s.data[end] <= ' ') end--;
    int64_t new_len = end - start + 1;
    if (new_len <= 0) return vbc_str_empty();
    return vbc_str_substring(stack, s, start, new_len);
}

static inline vbc_string_t vbc_str_trim_start(vbc_stack_t* stack, vbc_string_t s) {
    if (!s.data || s.len == 0) return vbc_str_empty();
    int64_t start = 0;
    while (start < s.len && (unsigned char)s.data[start] <= ' ') start++;
    return vbc_str_substring(stack, s, start, s.len - start);
}

static inline vbc_string_t vbc_str_trim_end(vbc_stack_t* stack, vbc_string_t s) {
    if (!s.data || s.len == 0) return vbc_str_empty();
    int64_t end = s.len - 1;
    while (end >= 0 && (unsigned char)s.data[end] <= ' ') end--;
    return vbc_str_substring(stack, s, 0, end + 1);
}

static inline vbc_string_t vbc_str_to_lower(vbc_stack_t* stack, vbc_string_t s) {
    if (!s.data || s.len == 0) return vbc_str_empty();
    vbc_string_t res = vbc_str_alloc(stack, s.len);
    char* dst = (char*)res.data;
    for (int64_t i = 0; i < s.len; i++) {
        dst[i] = (char)tolower((unsigned char)s.data[i]);
    }
    return res;
}

static inline vbc_string_t vbc_str_to_upper(vbc_stack_t* stack, vbc_string_t s) {
    if (!s.data || s.len == 0) return vbc_str_empty();
    vbc_string_t res = vbc_str_alloc(stack, s.len);
    char* dst = (char*)res.data;
    for (int64_t i = 0; i < s.len; i++) {
        dst[i] = (char)toupper((unsigned char)s.data[i]);
    }
    return res;
}

static inline vbc_string_t vbc_str_replace(vbc_stack_t* stack, vbc_string_t s, vbc_string_t old_str, vbc_string_t new_str) {
    if (!s.data || s.len == 0 || old_str.len == 0) return s;
    int64_t count = 0;
    int64_t pos = 0;
    while ((pos = vbc_str_index_of(s, old_str, pos)) >= 0) {
        count++;
        pos += old_str.len;
    }
    if (count == 0) return s;
    int64_t new_len = s.len + count * (new_str.len - old_str.len);
    vbc_string_t res = vbc_str_alloc(stack, new_len);
    char* dst = (char*)res.data;
    int64_t src_i = 0, dst_i = 0;
    while (src_i < s.len) {
        if (src_i <= s.len - old_str.len && memcmp(s.data + src_i, old_str.data, (size_t)old_str.len) == 0) {
            if (new_str.len > 0) {
                memcpy(dst + dst_i, new_str.data, (size_t)new_str.len);
                dst_i += new_str.len;
            }
            src_i += old_str.len;
        } else {
            dst[dst_i++] = s.data[src_i++];
        }
    }
    return res;
}

static inline vbc_slice_t vbc_str_split(vbc_stack_t* stack, vbc_string_t s, vbc_string_t delim) {
    vbc_slice_t slice = vbc_slice_create(stack, 4, sizeof(vbc_string_t));
    if (!s.data || s.len == 0) return slice;
    if (delim.len == 0) {
        vbc_slice_push(stack, &slice, &s);
        return slice;
    }
    int64_t start = 0;
    while (start <= s.len) {
        int64_t idx = vbc_str_index_of(s, delim, start);
        if (idx < 0) idx = s.len;
        vbc_string_t part = vbc_str_substring(stack, s, start, idx - start);
        vbc_slice_push(stack, &slice, &part);
        start = idx + delim.len;
        if (start > s.len && idx < s.len) {
            vbc_string_t empty = vbc_str_empty();
            vbc_slice_push(stack, &slice, &empty);
            break;
        }
    }
    return slice;
}

static inline vbc_string_t vbc_str_join(vbc_stack_t* stack, vbc_string_t delim, vbc_slice_t items) {
    if (items.len <= 0 || !items.data) return vbc_str_empty();
    const vbc_string_t* strs = (const vbc_string_t*)items.data;
    int64_t total_len = 0;
    for (int64_t i = 0; i < items.len; i++) {
        total_len += strs[i].len;
        if (i < items.len - 1) total_len += delim.len;
    }
    vbc_string_t res = vbc_str_alloc(stack, total_len);
    char* dst = (char*)res.data;
    int64_t cur = 0;
    for (int64_t i = 0; i < items.len; i++) {
        if (strs[i].len > 0) {
            memcpy(dst + cur, strs[i].data, (size_t)strs[i].len);
            cur += strs[i].len;
        }
        if (i < items.len - 1 && delim.len > 0) {
            memcpy(dst + cur, delim.data, (size_t)delim.len);
            cur += delim.len;
        }
    }
    return res;
}

static inline vbc_string_t vbc_str_pad_left(vbc_stack_t* stack, vbc_string_t s, int64_t total_width, char pad_char) {
    if (s.len >= total_width) return s;
    int64_t pad_len = total_width - s.len;
    vbc_string_t res = vbc_str_alloc(stack, total_width);
    char* dst = (char*)res.data;
    memset(dst, pad_char, (size_t)pad_len);
    if (s.len > 0) memcpy(dst + pad_len, s.data, (size_t)s.len);
    return res;
}

static inline vbc_string_t vbc_str_pad_right(vbc_stack_t* stack, vbc_string_t s, int64_t total_width, char pad_char) {
    if (s.len >= total_width) return s;
    int64_t pad_len = total_width - s.len;
    vbc_string_t res = vbc_str_alloc(stack, total_width);
    char* dst = (char*)res.data;
    if (s.len > 0) memcpy(dst, s.data, (size_t)s.len);
    memset(dst + s.len, pad_char, (size_t)pad_len);
    return res;
}

/* ========================================================================= */
/* System.DateTime & System.TimeSpan & System.Diagnostics.Stopwatch          */
/* ========================================================================= */

typedef struct {
    int64_t ticks;
    int32_t year;
    int32_t month;
    int32_t day;
    int32_t hour;
    int32_t minute;
    int32_t second;
    int32_t millisecond;
    int32_t day_of_week;
} vbc_datetime_t;

typedef struct {
    int64_t ticks;
} vbc_timespan_t;

static inline vbc_datetime_t vbc_datetime_from_tm(struct tm* t, int ms) {
    vbc_datetime_t dt;
    dt.year = t->tm_year + 1900;
    dt.month = t->tm_mon + 1;
    dt.day = t->tm_mday;
    dt.hour = t->tm_hour;
    dt.minute = t->tm_min;
    dt.second = t->tm_sec;
    dt.millisecond = ms;
    dt.day_of_week = t->tm_wday;
    dt.ticks = ((int64_t)mktime(t) * 10000000LL) + ((int64_t)ms * 10000LL);
    return dt;
}

static inline vbc_datetime_t vbc_datetime_now(void) {
#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    vbc_datetime_t dt;
    dt.year = st.wYear;
    dt.month = st.wMonth;
    dt.day = st.wDay;
    dt.hour = st.wHour;
    dt.minute = st.wMinute;
    dt.second = st.wSecond;
    dt.millisecond = st.wMilliseconds;
    dt.day_of_week = st.wDayOfWeek;
    dt.ticks = 0;
    return dt;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* t = localtime(&tv.tv_sec);
    return vbc_datetime_from_tm(t, (int)(tv.tv_usec / 1000));
#endif
}

static inline vbc_datetime_t vbc_datetime_utcnow(void) {
#ifdef _WIN32
    SYSTEMTIME st;
    GetSystemTime(&st);
    vbc_datetime_t dt;
    dt.year = st.wYear;
    dt.month = st.wMonth;
    dt.day = st.wDay;
    dt.hour = st.wHour;
    dt.minute = st.wMinute;
    dt.second = st.wSecond;
    dt.millisecond = st.wMilliseconds;
    dt.day_of_week = st.wDayOfWeek;
    dt.ticks = 0;
    return dt;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* t = gmtime(&tv.tv_sec);
    return vbc_datetime_from_tm(t, (int)(tv.tv_usec / 1000));
#endif
}

static inline vbc_datetime_t vbc_datetime_today(void) {
    vbc_datetime_t dt = vbc_datetime_now();
    dt.hour = 0;
    dt.minute = 0;
    dt.second = 0;
    dt.millisecond = 0;
    return dt;
}

static inline vbc_string_t vbc_datetime_to_string(vbc_stack_t* stack, vbc_datetime_t dt, vbc_string_t format) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                       dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    (void)format;
    vbc_string_t res = vbc_str_alloc(stack, len);
    memcpy((char*)res.data, buf, (size_t)len);
    return res;
}

static inline vbc_timespan_t vbc_timespan_from_seconds(double s) {
    vbc_timespan_t ts;
    ts.ticks = (int64_t)(s * 10000000.0);
    return ts;
}

static inline vbc_timespan_t vbc_timespan_from_milliseconds(double ms) {
    vbc_timespan_t ts;
    ts.ticks = (int64_t)(ms * 10000.0);
    return ts;
}

static inline double vbc_timespan_total_seconds(vbc_timespan_t ts) {
    return (double)ts.ticks / 10000000.0;
}

static inline double vbc_timespan_total_milliseconds(vbc_timespan_t ts) {
    return (double)ts.ticks / 10000.0;
}

typedef struct {
    int64_t start_time;
    int64_t elapsed;
    bool is_running;
} vbc_stopwatch_t;

static inline int64_t vbc_get_hr_time_ns(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq;
    static int init = 0;
    if (!init) { QueryPerformanceFrequency(&freq); init = 1; }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (int64_t)((counter.QuadPart * 1000000000LL) / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
#endif
}

static inline vbc_stopwatch_t vbc_stopwatch_start_new(void) {
    vbc_stopwatch_t sw;
    sw.start_time = vbc_get_hr_time_ns();
    sw.elapsed = 0;
    sw.is_running = true;
    return sw;
}

static inline void vbc_stopwatch_stop(vbc_stopwatch_t* sw) {
    if (sw->is_running) {
        sw->elapsed += vbc_get_hr_time_ns() - sw->start_time;
        sw->is_running = false;
    }
}

static inline int64_t vbc_stopwatch_elapsed_ms(const vbc_stopwatch_t* sw) {
    int64_t total = sw->elapsed;
    if (sw->is_running) total += vbc_get_hr_time_ns() - sw->start_time;
    return total / 1000000LL;
}

static inline void vbc_stopwatch_restart(vbc_stopwatch_t* sw) {
    sw->start_time = vbc_get_hr_time_ns();
    sw->elapsed = 0;
    sw->is_running = true;
}

/* ========================================================================= */
/* System.IO.Path & File Extensions                                          */
/* ========================================================================= */

static inline vbc_string_t vbc_path_combine(vbc_stack_t* stack, vbc_string_t p1, vbc_string_t p2) {
    if (p1.len == 0) return p2;
    if (p2.len == 0) return p1;
    bool needs_sep = (p1.data[p1.len - 1] != '/' && p1.data[p1.len - 1] != '\\');
    int64_t total = p1.len + (needs_sep ? 1 : 0) + p2.len;
    vbc_string_t res = vbc_str_alloc(stack, total);
    char* dst = (char*)res.data;
    memcpy(dst, p1.data, (size_t)p1.len);
    if (needs_sep) dst[p1.len] = '/';
    memcpy(dst + p1.len + (needs_sep ? 1 : 0), p2.data, (size_t)p2.len);
    return res;
}

static inline vbc_string_t vbc_path_get_file_name(vbc_stack_t* stack, vbc_string_t p) {
    if (p.len == 0) return p;
    int64_t idx = p.len - 1;
    while (idx >= 0 && p.data[idx] != '/' && p.data[idx] != '\\') idx--;
    return vbc_str_substring(stack, p, idx + 1, p.len - (idx + 1));
}

static inline vbc_string_t vbc_path_get_extension(vbc_stack_t* stack, vbc_string_t p) {
    vbc_string_t fname = vbc_path_get_file_name(stack, p);
    int64_t dot = vbc_str_last_index_of(fname, vbc_str_lit("."));
    if (dot < 0) return vbc_str_empty();
    return vbc_str_substring(stack, fname, dot, fname.len - dot);
}

static inline vbc_string_t vbc_path_get_directory_name(vbc_stack_t* stack, vbc_string_t p) {
    if (p.len == 0) return p;
    int64_t idx = p.len - 1;
    while (idx >= 0 && p.data[idx] != '/' && p.data[idx] != '\\') idx--;
    if (idx <= 0) return vbc_str_lit(".");
    return vbc_str_substring(stack, p, 0, idx);
}

static inline bool vbc_file_delete(vbc_string_t p) {
    char buf[1024];
    int64_t copy_len = p.len < 1023 ? p.len : 1023;
    memcpy(buf, p.data, (size_t)copy_len);
    buf[copy_len] = '\0';
    return remove(buf) == 0;
}

static inline bool vbc_file_copy(vbc_string_t src, vbc_string_t dst, bool overwrite) {
    char sbuf[1024], dbuf[1024];
    int64_t slen = src.len < 1023 ? src.len : 1023;
    int64_t dlen = dst.len < 1023 ? dst.len : 1023;
    memcpy(sbuf, src.data, (size_t)slen); sbuf[slen] = '\0';
    memcpy(dbuf, dst.data, (size_t)dlen); dbuf[dlen] = '\0';
#ifdef _WIN32
    return CopyFileA(sbuf, dbuf, !overwrite) != 0;
#else
    FILE* fsrc = fopen(sbuf, "rb");
    if (!fsrc) return false;
    if (!overwrite) {
        FILE* chk = fopen(dbuf, "rb");
        if (chk) { fclose(chk); fclose(fsrc); return false; }
    }
    FILE* fdst = fopen(dbuf, "wb");
    if (!fdst) { fclose(fsrc); return false; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fsrc)) > 0) {
        fwrite(buf, 1, n, fdst);
    }
    fclose(fsrc);
    fclose(fdst);
    return true;
#endif
}

static inline vbc_slice_t vbc_file_read_all_bytes(vbc_stack_t* stack, vbc_string_t path) {
    vbc_slice_t slice = vbc_slice_create(stack, 1, sizeof(uint8_t));
    char buf[1024];
    int64_t copy_len = path.len < 1023 ? path.len : 1023;
    memcpy(buf, path.data, (size_t)copy_len);
    buf[copy_len] = '\0';
    FILE* f = fopen(buf, "rb");
    if (!f) return slice;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return slice; }
    slice = vbc_slice_create(stack, sz, sizeof(uint8_t));
    slice.len = sz;
    size_t rd = fread(slice.data, 1, (size_t)sz, f);
    (void)rd;
    fclose(f);
    return slice;
}

static inline bool vbc_file_write_all_bytes(vbc_string_t path, vbc_slice_t bytes) {
    char buf[1024];
    int64_t copy_len = path.len < 1023 ? path.len : 1023;
    memcpy(buf, path.data, (size_t)copy_len);
    buf[copy_len] = '\0';
    FILE* f = fopen(buf, "wb");
    if (!f) return false;
    if (bytes.len > 0 && bytes.data) {
        fwrite(bytes.data, 1, (size_t)bytes.len, f);
    }
    fclose(f);
    return true;
}

/* ========================================================================= */
/* System.Text.StringBuilder                                                 */
/* ========================================================================= */

typedef struct {
    vbc_stack_t* stack;
    char* data;
    int64_t len;
    int64_t capacity;
} vbc_string_builder_t;

static inline vbc_string_builder_t vbc_sb_create(vbc_stack_t* stack, int64_t initial_cap) {
    vbc_string_builder_t sb;
    sb.stack = stack;
    sb.capacity = initial_cap > 16 ? initial_cap : 16;
    sb.len = 0;
    sb.data = (char*)vbc_stack_alloc(stack, (size_t)sb.capacity, 8);
    if (sb.data) sb.data[0] = '\0';
    return sb;
}

static inline void vbc_sb_append(vbc_string_builder_t* sb, vbc_string_t str) {
    if (str.len <= 0 || !str.data) return;
    if (sb->len + str.len + 1 > sb->capacity) {
        int64_t new_cap = (sb->capacity * 2 > sb->len + str.len + 16) ? sb->capacity * 2 : (sb->len + str.len + 32);
        char* new_buf = (char*)vbc_stack_alloc(sb->stack, (size_t)new_cap, 8);
        if (new_buf && sb->data) {
            memcpy(new_buf, sb->data, (size_t)sb->len);
        }
        sb->data = new_buf;
        sb->capacity = new_cap;
    }
    if (sb->data) {
        memcpy(sb->data + sb->len, str.data, (size_t)str.len);
        sb->len += str.len;
        sb->data[sb->len] = '\0';
    }
}

static inline void vbc_sb_append_line(vbc_string_builder_t* sb, vbc_string_t str) {
    vbc_sb_append(sb, str);
    vbc_sb_append(sb, vbc_str_lit("\n"));
}

static inline void vbc_sb_clear(vbc_string_builder_t* sb) {
    sb->len = 0;
    if (sb->data) sb->data[0] = '\0';
}

static inline vbc_string_t vbc_sb_to_string(const vbc_string_builder_t* sb) {
    if (!sb->data || sb->len <= 0) return vbc_str_empty();
    return vbc_str_from_slice(sb->data, sb->len);
}

/* ========================================================================= */
/* System.Guid & System.Environment                                          */
/* ========================================================================= */

static inline vbc_string_t vbc_guid_new_guid(vbc_stack_t* stack) {
    uint8_t bytes[16];
#ifdef _WIN32
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        CryptGenRandom(hProv, 16, bytes);
        CryptReleaseContext(hProv, 0);
    }
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t rd = read(fd, bytes, 16);
        (void)rd;
        close(fd);
    }
#endif
    // Set version 4 (random) and variant (RFC 4122)
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    char buf[40];
    int len = snprintf(buf, sizeof(buf),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2], bytes[3],
             bytes[4], bytes[5], bytes[6], bytes[7],
             bytes[8], bytes[9], bytes[10], bytes[11],
             bytes[12], bytes[13], bytes[14], bytes[15]);

    vbc_string_t res = vbc_str_alloc(stack, len);
    memcpy((char*)res.data, buf, (size_t)len);
    return res;
}

static inline vbc_string_t vbc_env_get_variable(vbc_stack_t* stack, vbc_string_t name) {
    char nbuf[256];
    int64_t nlen = name.len < 255 ? name.len : 255;
    memcpy(nbuf, name.data, (size_t)nlen);
    nbuf[nlen] = '\0';
    const char* val = getenv(nbuf);
    if (!val) return vbc_str_empty();
    size_t vlen = strlen(val);
    vbc_string_t res = vbc_str_alloc(stack, (int64_t)vlen);
    memcpy((char*)res.data, val, vlen);
    return res;
}

static inline int32_t vbc_env_processor_count(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int32_t)si.dwNumberOfProcessors;
#else
    return (int32_t)sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

static inline int64_t vbc_env_tick_count_64(void) {
#ifdef _WIN32
    return (int64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000LL + (int64_t)ts.tv_nsec / 1000000LL;
#endif
}

/* ========================================================================= */
/* Nullable Value Types Representation & Generic Interface Fat Pointer       */
/* ========================================================================= */

typedef struct { int32_t value; bool has_value; } vbc_nullable_int32_t;
typedef struct { int64_t value; bool has_value; } vbc_nullable_int64_t;
typedef struct { bool value; bool has_value; } vbc_nullable_bool;
typedef struct { double value; bool has_value; } vbc_nullable_double;
typedef struct { uint8_t value; bool has_value; } vbc_nullable_uint8_t;

/* Fat Pointer representing an Interface: object instance + interface itable */
typedef struct {
    void* obj;
    const void* _vtable;
} vbc_iface_t;

#ifdef __cplusplus
}
#endif

#endif /* VBC_BCL_H */
