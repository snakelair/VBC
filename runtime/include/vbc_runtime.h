#ifndef VBC_RUNTIME_H
#define VBC_RUNTIME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* Linear Arena Data Stack (Bump Allocator)                                  */
/* ========================================================================= */

typedef struct {
    char* base;
    char* ptr;
    size_t capacity;
    size_t committed;
    size_t high_watermark;
} vbc_stack_t;

static inline void vbc_stack_init(vbc_stack_t* stack, size_t capacity) {
    if (capacity == 0) capacity = 64 * 1024 * 1024; // Default 64 MB virtual address space
#ifdef _WIN32
    stack->base = (char*)VirtualAlloc(NULL, capacity, MEM_RESERVE, PAGE_READWRITE);
    if (!stack->base) {
        stack->base = (char*)malloc(capacity);
        stack->committed = capacity;
    } else {
        size_t initial_commit = 64 * 1024; // Commit first 64 KB
        VirtualAlloc(stack->base, initial_commit, MEM_COMMIT, PAGE_READWRITE);
        stack->committed = initial_commit;
    }
#else
    stack->base = (char*)mmap(NULL, capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack->base == MAP_FAILED) stack->base = NULL;
    if (!stack->base) stack->base = (char*)malloc(capacity);
    stack->committed = capacity;
#endif
    stack->ptr = stack->base;
    stack->capacity = capacity;
    stack->high_watermark = 0;
}

static inline void vbc_stack_free(vbc_stack_t* stack) {
    if (!stack || !stack->base) return;
#ifdef _WIN32
    VirtualFree(stack->base, 0, MEM_RELEASE);
#else
    munmap(stack->base, stack->capacity);
#endif
    stack->base = NULL;
    stack->ptr = NULL;
    stack->capacity = 0;
    stack->committed = 0;
}

static inline size_t vbc_stack_mark(const vbc_stack_t* stack) {
    return (size_t)(stack->ptr - stack->base);
}

static inline void* vbc_stack_alloc(vbc_stack_t* stack, size_t size, size_t align) {
    if (align == 0) align = sizeof(void*);
    uintptr_t current = (uintptr_t)stack->ptr;
    uintptr_t aligned = (current + (align - 1)) & ~(uintptr_t)(align - 1);
    size_t padding = aligned - current;
    size_t total_needed = size + padding;
    size_t needed = (size_t)(stack->ptr - stack->base) + total_needed;

    if (needed > stack->capacity) {
        fprintf(stderr, "VBC Runtime Error: Data stack overflow (capacity %zu bytes exceeded)\n", stack->capacity);
        abort();
    }

#ifdef _WIN32
    if (needed > stack->committed) {
        size_t chunk = 1024 * 1024; // 1 MB chunk
        size_t new_committed = (needed + chunk - 1) & ~(chunk - 1);
        if (new_committed > stack->capacity) new_committed = stack->capacity;
        VirtualAlloc(stack->base + stack->committed, new_committed - stack->committed, MEM_COMMIT, PAGE_READWRITE);
        stack->committed = new_committed;
    }
#endif

    stack->ptr = (char*)(aligned + size);
    size_t used = (size_t)(stack->ptr - stack->base);
    if (used > stack->high_watermark) {
        stack->high_watermark = used;
    }
    return (void*)aligned;
}

// O(1) scope-exit instant reclamation (Linear stack principle)
static inline void vbc_stack_reset(vbc_stack_t* stack, size_t mark) {
    stack->ptr = stack->base + mark;
}

/* ========================================================================= */
/* VBC Contiguous String Type                                                */
/* ========================================================================= */

typedef struct {
    const char* data;
    int64_t len;
} vbc_string_t;

static inline vbc_string_t vbc_str_empty(void) {
    vbc_string_t s = { "", 0 };
    return s;
}

static inline vbc_string_t vbc_str_lit(const char* lit) {
    vbc_string_t s;
    s.data = lit ? lit : "";
    s.len = lit ? (int64_t)strlen(lit) : 0;
    return s;
}

static inline vbc_string_t vbc_str_from_slice(const char* data, int64_t len) {
    vbc_string_t s;
    s.data = data ? data : "";
    s.len = len > 0 ? len : 0;
    return s;
}

static inline vbc_string_t vbc_str_from_cstr_len(const char* data, int64_t len) {
    return vbc_str_from_slice(data, len);
}

static inline vbc_string_t vbc_str_alloc(vbc_stack_t* stack, int64_t len) {
    vbc_string_t s;
    char* buf = (char*)vbc_stack_alloc(stack, (size_t)len + 1, 1);
    buf[len] = '\0';
    s.data = buf;
    s.len = len;
    return s;
}

static inline vbc_string_t vbc_str_concat(vbc_stack_t* stack, vbc_string_t a, vbc_string_t b) {
    int64_t new_len = a.len + b.len;
    if (stack != NULL && a.data != NULL && a.len > 0 && ((char*)a.data + a.len + 1 == stack->ptr)) {
        size_t needed = (size_t)(stack->ptr - stack->base) + (size_t)b.len;
        if (needed <= stack->capacity) {
#ifdef _WIN32
            if (needed > stack->committed) {
                size_t chunk = 1024 * 1024;
                size_t new_committed = (needed + chunk - 1) & ~(chunk - 1);
                if (new_committed > stack->capacity) new_committed = stack->capacity;
                VirtualAlloc(stack->base + stack->committed, new_committed - stack->committed, MEM_COMMIT, PAGE_READWRITE);
                stack->committed = new_committed;
            }
#endif
            char* dest = (char*)a.data + a.len;
            if (b.len > 0 && b.data) memcpy(dest, b.data, (size_t)b.len);
            dest[b.len] = '\0';
            stack->ptr = (char*)a.data + new_len + 1;
            size_t used = (size_t)(stack->ptr - stack->base);
            if (used > stack->high_watermark) stack->high_watermark = used;
            a.len = new_len;
            return a;
        }
    }
    vbc_string_t s = vbc_str_alloc(stack, new_len);
    char* dest = (char*)s.data;
    if (a.len > 0 && a.data) memcpy(dest, a.data, (size_t)a.len);
    if (b.len > 0 && b.data) memcpy(dest + a.len, b.data, (size_t)b.len);
    dest[new_len] = '\0';
    return s;
}

static inline vbc_string_t vbc_str_concat3(vbc_stack_t* stack, vbc_string_t a, vbc_string_t b, vbc_string_t c) {
    int64_t new_len = a.len + b.len + c.len;
    vbc_string_t s = vbc_str_alloc(stack, new_len);
    char* dest = (char*)s.data;
    if (a.len > 0) memcpy(dest, a.data, (size_t)a.len);
    if (b.len > 0) memcpy(dest + a.len, b.data, (size_t)b.len);
    if (c.len > 0) memcpy(dest + a.len + b.len, c.data, (size_t)c.len);
    dest[new_len] = '\0';
    return s;
}

static inline bool vbc_str_equals(vbc_string_t a, vbc_string_t b) {
    if (a.len != b.len) return false;
    if (a.len == 0) return true;
    return memcmp(a.data, b.data, (size_t)a.len) == 0;
}

static inline int vbc_str_compare(vbc_string_t a, vbc_string_t b) {
    int64_t min_len = a.len < b.len ? a.len : b.len;
    int cmp = memcmp(a.data, b.data, (size_t)min_len);
    if (cmp != 0) return cmp;
    if (a.len < b.len) return -1;
    if (a.len > b.len) return 1;
    return 0;
}

static inline vbc_string_t vbc_i32_to_str(vbc_stack_t* stack, int32_t val) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d", val);
    vbc_string_t s = vbc_str_alloc(stack, len);
    memcpy((char*)s.data, buf, len);
    return s;
}

static inline vbc_string_t vbc_i64_to_str(vbc_stack_t* stack, int64_t val) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%lld", (long long)val);
    vbc_string_t s = vbc_str_alloc(stack, len);
    memcpy((char*)s.data, buf, len);
    return s;
}

static inline vbc_string_t vbc_f64_to_str(vbc_stack_t* stack, double val) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%g", val);
    vbc_string_t s = vbc_str_alloc(stack, len);
    memcpy((char*)s.data, buf, len);
    return s;
}

static inline vbc_string_t vbc_bool_to_str(bool val) {
    return val ? vbc_str_lit("True") : vbc_str_lit("False");
}

static inline vbc_string_t vbc_str_substring(vbc_stack_t* stack, vbc_string_t s, int64_t start, int64_t len) {
    if (start < 0) start = 0;
    if (start > s.len) start = s.len;
    if (len < 0 || start + len > s.len) len = s.len - start;
    vbc_string_t res = vbc_str_alloc(stack, len);
    if (len > 0) memcpy((char*)res.data, s.data + start, (size_t)len);
    return res;
}

static inline int64_t vbc_str_index_of(vbc_string_t s, vbc_string_t target, int64_t start) {
    if (start < 0) start = 0;
    if (target.len == 0) return start <= s.len ? start : -1;
    if (start + target.len > s.len) return -1;
    for (int64_t i = start; i <= s.len - target.len; i++) {
        if (memcmp(s.data + i, target.data, (size_t)target.len) == 0) return i;
    }
    return -1;
}

static inline int32_t vbc_str_byte_at(vbc_string_t s, int64_t index) {
    if (index < 0 || index >= s.len) return -1;
    return (uint8_t)s.data[index];
}

/* ========================================================================= */
/* VBC Exception Handling System (setjmp/longjmp)                            */
/* ========================================================================= */

typedef struct {
    vbc_string_t Message;
    int32_t Code;
} vbc_exception_t;

typedef vbc_exception_t Vbc_Exception;

typedef struct vbc_exception_frame {
    jmp_buf jmp;
    struct vbc_exception_frame* prev;
    vbc_exception_t exception;
} vbc_exception_frame_t;

#if defined(__GNUC__) || defined(__clang__)
static __thread vbc_exception_frame_t* _vbc_top_exception_frame = NULL;
#elif defined(_MSC_VER)
static __declspec(thread) vbc_exception_frame_t* _vbc_top_exception_frame = NULL;
#else
static _Thread_local vbc_exception_frame_t* _vbc_top_exception_frame = NULL;
#endif

static inline void vbc_push_exception_frame(vbc_exception_frame_t* frame) {
    frame->prev = _vbc_top_exception_frame;
    frame->exception.Message = (vbc_string_t){ "", 0 };
    frame->exception.Code = 0;
    _vbc_top_exception_frame = frame;
}

static inline void vbc_pop_exception_frame(vbc_exception_frame_t* frame) {
    if (_vbc_top_exception_frame == frame) {
        _vbc_top_exception_frame = frame->prev;
    }
}

static inline void vbc_throw(vbc_string_t msg, int32_t code) {
    if (_vbc_top_exception_frame != NULL) {
        _vbc_top_exception_frame->exception.Message = msg;
        _vbc_top_exception_frame->exception.Code = code;
        longjmp(_vbc_top_exception_frame->jmp, 1);
    } else {
        fprintf(stderr, "Unhandled Exception: %.*s (code %d)\n", (int)msg.len, msg.data ? msg.data : "", code);
        abort();
    }
}

static inline void vbc_rethrow(vbc_exception_t ex) {
    vbc_throw(ex.Message, ex.Code);
}

/* ========================================================================= */
/* VBC Dynamic Slice / Array Type                                            */
/* ========================================================================= */

typedef struct {
    void* data;
    int64_t len;
    int64_t cap;
    int64_t elem_size;
} vbc_slice_t;

static inline vbc_slice_t vbc_slice_create(vbc_stack_t* stack, int64_t initial_cap, int64_t elem_size) {
    vbc_slice_t slice;
    slice.len = 0;
    slice.elem_size = elem_size;
    if (elem_size > 0) {
        slice.cap = initial_cap > 4 ? initial_cap : 4;
        slice.data = vbc_stack_alloc(stack, (size_t)(slice.cap * elem_size), (size_t)elem_size);
    } else {
        slice.cap = 0;
        slice.data = NULL;
    }
    return slice;
}

static inline vbc_slice_t vbc_slice_create_zeroed(vbc_stack_t* stack, int64_t len, int64_t elem_size) {
    vbc_slice_t slice;
    slice.len = len;
    slice.cap = len > 4 ? len : 4;
    slice.elem_size = elem_size;
    slice.data = vbc_stack_alloc(stack, (size_t)(slice.cap * elem_size), (size_t)elem_size);
    memset(slice.data, 0, (size_t)(slice.cap * elem_size));
    return slice;
}

static inline void* vbc_slice_push(vbc_stack_t* stack, vbc_slice_t* slice, const void* elem) {
    if (slice->elem_size <= 0) slice->elem_size = sizeof(int32_t);
    if (slice->len >= slice->cap || slice->data == NULL) {
        int64_t new_cap = slice->cap > 0 ? slice->cap * 2 : 8;
        void* new_data = vbc_stack_alloc(stack, (size_t)(new_cap * slice->elem_size), (size_t)slice->elem_size);
        if (slice->data && slice->len > 0) {
            memcpy(new_data, slice->data, (size_t)(slice->len * slice->elem_size));
        }
        slice->data = new_data;
        slice->cap = new_cap;
    }
    char* target = (char*)slice->data + (slice->len * slice->elem_size);
    if (elem) {
        memcpy(target, elem, (size_t)slice->elem_size);
    } else {
        memset(target, 0, (size_t)slice->elem_size);
    }
    slice->len++;
    return target;
}

static inline void vbc_slice_push_i32(vbc_stack_t* stack, vbc_slice_t* slice, int32_t val) {
    slice->elem_size = sizeof(int32_t);
    vbc_slice_push(stack, slice, &val);
}

static inline void vbc_slice_push_i64(vbc_stack_t* stack, vbc_slice_t* slice, int64_t val) {
    slice->elem_size = sizeof(int64_t);
    vbc_slice_push(stack, slice, &val);
}

static inline void vbc_slice_push_f64(vbc_stack_t* stack, vbc_slice_t* slice, double val) {
    slice->elem_size = sizeof(double);
    vbc_slice_push(stack, slice, &val);
}

static inline void* vbc_slice_get(const vbc_slice_t* slice, int64_t index) {
    if (index < 0 || index >= slice->len) {
        fprintf(stderr, "VBC Runtime Error: Index %lld out of bounds [0, %lld)\n", (long long)index, (long long)slice->len);
        abort();
    }
    return (char*)slice->data + (index * slice->elem_size);
}

/* ========================================================================= */
/* Standard Console Output Functions                                         */
/* ========================================================================= */

static inline void vbc_print(vbc_string_t s) {
    if (s.len > 0 && s.data) {
        fwrite(s.data, 1, (size_t)s.len, stdout);
        fflush(stdout);
    }
}

static inline void vbc_println_str(vbc_string_t s) {
    vbc_print(s);
    fputc('\n', stdout);
    fflush(stdout);
}

static inline void vbc_println_i32(int32_t val) {
    printf("%d\n", val);
    fflush(stdout);
}

static inline void vbc_println_i64(int64_t val) {
    printf("%lld\n", (long long)val);
    fflush(stdout);
}

static inline void vbc_println_f64(double val) {
    printf("%g\n", val);
    fflush(stdout);
}

static inline void vbc_println_bool(bool val) {
    puts(val ? "True" : "False");
    fflush(stdout);
}

#define vbc_println(x) _Generic((x), \
    vbc_string_t: vbc_println_str, \
    int32_t: vbc_println_i32, \
    int64_t: vbc_println_i64, \
    double: vbc_println_f64, \
    float: vbc_println_f64, \
    bool: vbc_println_bool, \
    default: vbc_println_str \
)(x)

/* ========================================================================= */
/* File and Process Execution Support                                         */
/* ========================================================================= */

static inline vbc_string_t vbc_file_read_all_text(vbc_stack_t* stack, vbc_string_t path) {
    char path_buf[1024];
    size_t copy_len = path.len < 1023 ? (size_t)path.len : 1023;
    memcpy(path_buf, path.data, copy_len);
    path_buf[copy_len] = '\0';

    FILE* f = fopen(path_buf, "rb");
    if (!f) return vbc_str_empty();

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return vbc_str_empty(); }

    vbc_string_t res = vbc_str_alloc(stack, sz);
    size_t read_bytes = fread((char*)res.data, 1, (size_t)sz, f);
    ((char*)res.data)[read_bytes] = '\0';
    res.len = (int64_t)read_bytes;
    fclose(f);
    return res;
}

static inline bool vbc_file_write_all_text(vbc_string_t path, vbc_string_t content) {
    char path_buf[1024];
    size_t copy_len = path.len < 1023 ? (size_t)path.len : 1023;
    memcpy(path_buf, path.data, copy_len);
    path_buf[copy_len] = '\0';

    FILE* f = fopen(path_buf, "wb");
    if (!f) return false;

    if (content.len > 0 && content.data) {
        fwrite(content.data, 1, (size_t)content.len, f);
    }
    fclose(f);
    return true;
}


static inline bool vbc_dir_exists(vbc_string_t path) {
    char path_buf[1024];
    size_t copy_len = path.len < 1023 ? (size_t)path.len : 1023;
    memcpy(path_buf, path.data, copy_len);
    path_buf[copy_len] = '\0';
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path_buf);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path_buf, &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

static inline vbc_slice_t vbc_dir_get_files(vbc_stack_t* stack, vbc_string_t dir_path, vbc_string_t ext) {
    vbc_slice_t slice = vbc_slice_create(stack, 16, sizeof(vbc_string_t));
    char pattern[1024];
    size_t dlen = dir_path.len < 1000 ? (size_t)dir_path.len : 1000;
    memcpy(pattern, dir_path.data, dlen);
    if (dlen > 0 && pattern[dlen - 1] != '/' && pattern[dlen - 1] != '\\') {
        pattern[dlen++] = '/';
    }
    pattern[dlen++] = '*';
    if (ext.len > 0) {
        size_t elen = ext.len < (sizeof(pattern) - dlen - 1) ? (size_t)ext.len : (sizeof(pattern) - dlen - 1);
        memcpy(pattern + dlen, ext.data, elen);
        dlen += elen;
    }
    pattern[dlen] = '\0';

#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char full[2048];
                int flen = snprintf(full, sizeof(full), "%.*s/%s", (int)dir_path.len, dir_path.data, fd.cFileName);
                vbc_string_t s = vbc_str_alloc(stack, flen);
                memcpy((char*)s.data, full, flen);
                ((char*)s.data)[flen] = '\0';
                s.len = flen;
                vbc_slice_push(stack, &slice, &s);
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
#endif

    // Alphabetical sort for deterministic multi-file compilation order
    for (int64_t i = 0; i < slice.len; i++) {
        for (int64_t j = i + 1; j < slice.len; j++) {
            vbc_string_t* sa = (vbc_string_t*)vbc_slice_get(&slice, i);
            vbc_string_t* sb = (vbc_string_t*)vbc_slice_get(&slice, j);
            if (vbc_str_compare(*sa, *sb) > 0) {
                vbc_string_t tmp = *sa;
                *sa = *sb;
                *sb = tmp;
            }
        }
    }

    return slice;
}

static inline int32_t vbc_process_execute(vbc_string_t cmd) {
    char cmd_buf[4096];
    size_t copy_len = cmd.len < 4095 ? (size_t)cmd.len : 4095;
    memcpy(cmd_buf, cmd.data, copy_len);
    cmd_buf[copy_len] = '\0';

    return (int32_t)system(cmd_buf);
}

static inline int32_t vbc_compile_c(vbc_string_t c_file, vbc_string_t exe_file) {
    char cmd[4096];
    char c_buf[1024];
    char exe_buf[1024];
    size_t c_len = c_file.len < 1023 ? (size_t)c_file.len : 1023;
    size_t exe_len = exe_file.len < 1023 ? (size_t)exe_file.len : 1023;
    memcpy(c_buf, c_file.data, c_len); c_buf[c_len] = '\0';
    memcpy(exe_buf, exe_file.data, exe_len); exe_buf[exe_len] = '\0';

    const char* runtime_path = getenv("VBC_RUNTIME_PATH");
    char inc_flag[512];
    char sock_src[512];
    char http_src[512];
    char async_src[512];

    char auto_root[1024] = {0};
    if (!runtime_path || runtime_path[0] == '\0') {
#ifdef _WIN32
        GetModuleFileNameA(NULL, auto_root, sizeof(auto_root));
        char* last_slash = strrchr(auto_root, '\\');
        if (!last_slash) last_slash = strrchr(auto_root, '/');
        if (last_slash) {
            *last_slash = '\0';
            char* bin_slash = strrchr(auto_root, '\\');
            if (!bin_slash) bin_slash = strrchr(auto_root, '/');
            if (bin_slash && (_stricmp(bin_slash + 1, "bin") == 0)) {
                *bin_slash = '\0';
            }
        }
#else
        ssize_t len = readlink("/proc/self/exe", auto_root, sizeof(auto_root) - 1);
        if (len != -1) {
            auto_root[len] = '\0';
            char* last_slash = strrchr(auto_root, '/');
            if (last_slash) {
                *last_slash = '\0';
                char* bin_slash = strrchr(auto_root, '/');
                if (bin_slash && (strcmp(bin_slash + 1, "bin") == 0)) {
                    *bin_slash = '\0';
                }
            }
        }
#endif
    }

    if (runtime_path && runtime_path[0] != '\0') {
        snprintf(inc_flag, sizeof(inc_flag), "-I \"%s/include\"", runtime_path);
        snprintf(sock_src, sizeof(sock_src), "\"%s/src/vbc_socket.c\"", runtime_path);
        snprintf(http_src, sizeof(http_src), "\"%s/src/vbc_http.c\"", runtime_path);
        snprintf(async_src, sizeof(async_src), "\"%s/src/vbc_async.c\"", runtime_path);
    } else if (auto_root[0] != '\0') {
        snprintf(inc_flag, sizeof(inc_flag), "-I \"%s/runtime/include\"", auto_root);
        snprintf(sock_src, sizeof(sock_src), "\"%s/runtime/src/vbc_socket.c\"", auto_root);
        snprintf(http_src, sizeof(http_src), "\"%s/runtime/src/vbc_http.c\"", auto_root);
        snprintf(async_src, sizeof(async_src), "\"%s/runtime/src/vbc_async.c\"", auto_root);
    } else {
        snprintf(inc_flag, sizeof(inc_flag), "-I runtime/include");
        snprintf(sock_src, sizeof(sock_src), "runtime/src/vbc_socket.c");
        snprintf(http_src, sizeof(http_src), "runtime/src/vbc_http.c");
        snprintf(async_src, sizeof(async_src), "runtime/src/vbc_async.c");
    }

#ifdef _WIN32
    char pdb_buf[512];
    strncpy(pdb_buf, exe_buf, sizeof(pdb_buf));
    char* dot = strrchr(pdb_buf, '.');
    if (dot) strcpy(dot, ".pdb");
    else strncat(pdb_buf, ".pdb", sizeof(pdb_buf) - strlen(pdb_buf) - 1);

    snprintf(cmd, sizeof(cmd),
        "clang -O0 -w -g -gcodeview \"-Wl,--pdb=%s\" \"-Wl,--stack,0x8000000\" %s \"%s\" %s %s %s -lws2_32 -lkernel32 -o \"%s\"",
        pdb_buf, inc_flag, c_buf, sock_src, http_src, async_src, exe_buf);
#elif defined(__APPLE__)
    snprintf(cmd, sizeof(cmd),
        "clang -O3 -w -g %s \"%s\" %s %s %s -lpthread -o \"%s\"",
        inc_flag, c_buf, sock_src, http_src, async_src, exe_buf);
#else
    snprintf(cmd, sizeof(cmd),
        "clang -O3 -w -g %s \"%s\" %s %s %s -lpthread -lm -o \"%s\"",
        inc_flag, c_buf, sock_src, http_src, async_src, exe_buf);
#endif

    return (int32_t)system(cmd);
}

/* ========================================================================= */
/* Native Language Server Protocol (LSP) Support                             */
/* ========================================================================= */

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define vbc_strncasecmp _strnicmp
#else
#include <strings.h>
#define vbc_strncasecmp strncasecmp
#endif

static inline void vbc_lsp_init(void) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

static inline vbc_string_t vbc_lsp_read_msg(vbc_stack_t* stack) {
    char line[512];
    int64_t content_length = 0;
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == '\r' || line[0] == '\n' || strcmp(line, "\r\n") == 0) {
            if (content_length > 0) break;
        }
        if (vbc_strncasecmp(line, "Content-Length:", 15) == 0) {
            content_length = atoll(line + 15);
        }
    }
    if (content_length <= 0) return vbc_str_empty();

    vbc_string_t res = vbc_str_alloc(stack, content_length);
    size_t total_read = 0;
    while (total_read < (size_t)content_length) {
        size_t r = fread((char*)res.data + total_read, 1, (size_t)content_length - total_read, stdin);
        if (r == 0) break;
        total_read += r;
    }
    ((char*)res.data)[total_read] = '\0';
    res.len = (int64_t)total_read;
    return res;
}

static inline void vbc_lsp_send_msg(vbc_string_t json) {
    if (json.len <= 0 || !json.data) return;
    fprintf(stdout, "Content-Length: %lld\r\n\r\n", (long long)json.len);
    fwrite(json.data, 1, (size_t)json.len, stdout);
    fflush(stdout);
}

static inline vbc_string_t vbc_json_get_str(vbc_stack_t* stack, vbc_string_t json, vbc_string_t key) {
    if (!json.data || json.len <= 0 || !key.data || key.len <= 0) return vbc_str_empty();

    char needle[256];
    size_t klen = key.len < 250 ? (size_t)key.len : 250;
    needle[0] = '"';
    memcpy(needle + 1, key.data, klen);
    needle[klen + 1] = '"';
    needle[klen + 2] = '\0';
    size_t nlen = klen + 2;

    const char* p = json.data;
    const char* end = json.data + json.len;

    while (p + nlen < end) {
        if (memcmp(p, needle, nlen) == 0) {
            p += nlen;
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':')) p++;
            if (p < end && *p == '"') {
                p++;
                const char* val_start = p;
                while (p < end && *p != '"') {
                    if (*p == '\\' && p + 1 < end) p += 2;
                    else p++;
                }
                size_t raw_len = (size_t)(p - val_start);
                vbc_string_t res = vbc_str_alloc(stack, raw_len);
                char* dst = (char*)res.data;
                size_t dlen = 0;
                for (size_t i = 0; i < raw_len; i++) {
                    if (val_start[i] == '\\' && i + 1 < raw_len) {
                        char next = val_start[i+1];
                        if (next == 'n') { dst[dlen++] = '\n'; i++; }
                        else if (next == 'r') { dst[dlen++] = '\r'; i++; }
                        else if (next == 't') { dst[dlen++] = '\t'; i++; }
                        else if (next == '"') { dst[dlen++] = '"'; i++; }
                        else if (next == '\\') { dst[dlen++] = '\\'; i++; }
                        else { dst[dlen++] = val_start[i]; }
                    } else {
                        dst[dlen++] = val_start[i];
                    }
                }
                dst[dlen] = '\0';
                res.len = (int64_t)dlen;
                return res;
            }
            return vbc_str_empty();
        }
        p++;
    }
    return vbc_str_empty();
}

static inline int64_t vbc_json_get_int(vbc_string_t json, vbc_string_t key) {
    if (!json.data || json.len <= 0 || !key.data || key.len <= 0) return -1;

    char needle[256];
    size_t klen = key.len < 250 ? (size_t)key.len : 250;
    needle[0] = '"';
    memcpy(needle + 1, key.data, klen);
    needle[klen + 1] = '"';
    needle[klen + 2] = '\0';
    size_t nlen = klen + 2;

    const char* p = json.data;
    const char* end = json.data + json.len;

    while (p + nlen < end) {
        if (memcmp(p, needle, nlen) == 0) {
            p += nlen;
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':')) p++;
            bool neg = false;
            if (p < end && *p == '-') { neg = true; p++; }
            int64_t val = 0;
            bool found_digit = false;
            while (p < end && *p >= '0' && *p <= '9') {
                found_digit = true;
                val = val * 10 + (*p - '0');
                p++;
            }
            if (!found_digit) return -1;
            return neg ? -val : val;
        }
        p++;
    }
    return -1;
}

/* ========================================================================= */
/* Monitor Synchronization (SyncLock / Monitor)                              */
/* ========================================================================= */

#define VBC_MONITOR_BUCKETS 64

#ifdef _WIN32
static CRITICAL_SECTION _vbc_monitor_cs[VBC_MONITOR_BUCKETS];
static volatile LONG _vbc_monitor_initialized = 0;

static inline void _vbc_monitor_init_if_needed(void) {
    if (InterlockedCompareExchange(&_vbc_monitor_initialized, 1, 0) == 0) {
        for (int i = 0; i < VBC_MONITOR_BUCKETS; i++) {
            InitializeCriticalSection(&_vbc_monitor_cs[i]);
        }
    }
}

static inline void vbc_monitor_enter(void* obj) {
    _vbc_monitor_init_if_needed();
    size_t idx = ((uintptr_t)obj >> 4) % VBC_MONITOR_BUCKETS;
    EnterCriticalSection(&_vbc_monitor_cs[idx]);
}

static inline void vbc_monitor_exit(void* obj) {
    size_t idx = ((uintptr_t)obj >> 4) % VBC_MONITOR_BUCKETS;
    LeaveCriticalSection(&_vbc_monitor_cs[idx]);
}
#else
#include <pthread.h>
#include <unistd.h>
static pthread_mutex_t _vbc_monitor_mutex[VBC_MONITOR_BUCKETS];
static volatile int _vbc_monitor_initialized = 0;

static inline void _vbc_monitor_init_if_needed(void) {
    if (__sync_bool_compare_and_swap(&_vbc_monitor_initialized, 0, 1)) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        for (int i = 0; i < VBC_MONITOR_BUCKETS; i++) {
            pthread_mutex_init(&_vbc_monitor_mutex[i], &attr);
        }
        pthread_mutexattr_destroy(&attr);
    }
}

static inline void vbc_monitor_enter(void* obj) {
    _vbc_monitor_init_if_needed();
    size_t idx = ((uintptr_t)obj >> 4) % VBC_MONITOR_BUCKETS;
    pthread_mutex_lock(&_vbc_monitor_mutex[idx]);
}

static inline void vbc_monitor_exit(void* obj) {
    size_t idx = ((uintptr_t)obj >> 4) % VBC_MONITOR_BUCKETS;
    pthread_mutex_unlock(&_vbc_monitor_mutex[idx]);
}
#endif

/* ========================================================================= */
/* Thread Sleep Primitive                                                    */
/* ========================================================================= */

static inline void vbc_thread_sleep(int64_t ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)(ms * 1000));
#endif
}

/* ========================================================================= */
/* Generic Dictionary / Hash Map (System.Collections.Generic.Dictionary)     */
/* ========================================================================= */

typedef struct vbc_dict_entry {
    vbc_string_t key;
    vbc_string_t value;
    struct vbc_dict_entry* next;
} vbc_dict_entry_t;

typedef struct {
    vbc_dict_entry_t** buckets;
    size_t capacity;
    int64_t count;
} vbc_dict_t;

static inline uint32_t _vbc_hash_str(vbc_string_t s) {
    uint32_t h = 2166136261u;
    for (int64_t i = 0; i < s.len; i++) {
        h ^= (uint8_t)s.data[i];
        h *= 16777619u;
    }
    return h;
}

static inline vbc_dict_t* vbc_dict_create(vbc_stack_t* stack, size_t initial_cap) {
    if (initial_cap == 0) initial_cap = 16;
    vbc_dict_t* d = (vbc_dict_t*)vbc_stack_alloc(stack, sizeof(vbc_dict_t), sizeof(void*));
    d->capacity = initial_cap;
    d->count = 0;
    d->buckets = (vbc_dict_entry_t**)vbc_stack_alloc(stack, sizeof(vbc_dict_entry_t*) * initial_cap, sizeof(void*));
    memset(d->buckets, 0, sizeof(vbc_dict_entry_t*) * initial_cap);
    return d;
}

static inline void vbc_dict_set(vbc_stack_t* stack, vbc_dict_t* d, vbc_string_t key, vbc_string_t val) {
    if (!d || d->capacity == 0) return;
    uint32_t h = _vbc_hash_str(key) % d->capacity;
    vbc_dict_entry_t* e = d->buckets[h];
    while (e) {
        if (vbc_str_equals(e->key, key)) {
            e->value = val;
            return;
        }
        e = e->next;
    }
    vbc_dict_entry_t* ne = (vbc_dict_entry_t*)vbc_stack_alloc(stack, sizeof(vbc_dict_entry_t), sizeof(void*));
    ne->key = key;
    ne->value = val;
    ne->next = d->buckets[h];
    d->buckets[h] = ne;
    d->count++;
}

static inline vbc_string_t vbc_dict_get(vbc_dict_t* d, vbc_string_t key) {
    if (!d || d->capacity == 0) return vbc_str_empty();
    uint32_t h = _vbc_hash_str(key) % d->capacity;
    vbc_dict_entry_t* e = d->buckets[h];
    while (e) {
        if (vbc_str_equals(e->key, key)) return e->value;
        e = e->next;
    }
    return vbc_str_empty();
}

static inline bool vbc_dict_contains(vbc_dict_t* d, vbc_string_t key) {
    if (!d || d->capacity == 0) return false;
    uint32_t h = _vbc_hash_str(key) % d->capacity;
    vbc_dict_entry_t* e = d->buckets[h];
    while (e) {
        if (vbc_str_equals(e->key, key)) return true;
        e = e->next;
    }
    return false;
}

static inline int64_t vbc_dict_count(vbc_dict_t* d) {
    return d ? d->count : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* VBC_RUNTIME_H */
