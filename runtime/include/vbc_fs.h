#ifndef VBC_FS_H
#define VBC_FS_H

#include "vbc_runtime.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    vbc_string_t path;
    vbc_string_t filter;
    bool is_watching;
#ifdef _WIN32
    HANDLE hChange;
#else
    int inotify_fd;
    int watch_fd;
#endif
} vbc_fs_watcher_t;

static inline vbc_fs_watcher_t vbc_fs_watcher_create(vbc_string_t path, vbc_string_t filter) {
    vbc_fs_watcher_t w;
    memset(&w, 0, sizeof(w));
    w.path = path;
    w.filter = filter;
    w.is_watching = false;
    return w;
}

static inline bool vbc_fs_watcher_start(vbc_fs_watcher_t* w) {
    if (!w || w->path.len <= 0) return false;
    char path_buf[512];
    int64_t plen = w->path.len < 511 ? w->path.len : 511;
    memcpy(path_buf, w->path.data, (size_t)plen);
    path_buf[plen] = '\0';

#ifdef _WIN32
    w->hChange = FindFirstChangeNotificationA(
        path_buf,
        TRUE, // Watch subtree
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE
    );
    if (w->hChange != INVALID_HANDLE_VALUE) {
        w->is_watching = true;
        return true;
    }
#endif
    w->is_watching = true;
    return true;
}

static inline bool vbc_fs_watcher_has_changes(vbc_fs_watcher_t* w, int timeout_ms) {
    if (!w || !w->is_watching) return false;

#ifdef _WIN32
    if (w->hChange != INVALID_HANDLE_VALUE) {
        DWORD res = WaitForSingleObject(w->hChange, (DWORD)timeout_ms);
        if (res == WAIT_OBJECT_0) {
            FindNextChangeNotification(w->hChange);
            return true;
        }
    }
#endif
    return false;
}

static inline void vbc_fs_watcher_stop(vbc_fs_watcher_t* w) {
    if (!w || !w->is_watching) return;
#ifdef _WIN32
    if (w->hChange != INVALID_HANDLE_VALUE) {
        FindCloseChangeNotification(w->hChange);
        w->hChange = INVALID_HANDLE_VALUE;
    }
#endif
    w->is_watching = false;
}

#ifdef __cplusplus
}
#endif

#endif /* VBC_FS_H */
