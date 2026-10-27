#ifndef VBC_ASYNC_H
#define VBC_ASYNC_H

#include "vbc_runtime.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <ucontext.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    VBC_TASK_PENDING = 0,
    VBC_TASK_RUNNING = 1,
    VBC_TASK_COMPLETED = 2,
    VBC_TASK_FAULTED = 3
} vbc_task_status_t;

typedef struct vbc_task {
    vbc_task_status_t status;
    uint8_t result_storage[64];
    void* result_val;
    size_t result_size;
    vbc_exception_t exception;

#ifdef _WIN32
    LPVOID fiber;
#else
    ucontext_t ctx;
    void* stack_mem;
    size_t stack_size;
#endif

    vbc_stack_t data_stack;
    void* args;
    size_t args_size;
    void (*fiber_proc)(void*);

    struct vbc_task* waiting_on;
    struct vbc_task* next;
    struct vbc_task* all_next;
    uint64_t wake_time_ms;
    bool is_main;
    bool is_timer;
} vbc_task_t;

typedef struct {
#ifdef _WIN32
    LPVOID main_fiber;
#else
    ucontext_t main_ctx;
#endif
    vbc_task_t* current_task;
    vbc_task_t* ready_head;
    vbc_task_t* ready_tail;
    vbc_task_t* timer_head;
    vbc_task_t* all_tasks_head;
    bool initialized;
    bool is_running;
} vbc_scheduler_t;

void vbc_async_init(void);
void vbc_async_cleanup(void);

vbc_task_t* vbc_task_create(void (*fn)(void*), const void* args, size_t args_size);
void* vbc_task_await(vbc_task_t* cur_task, vbc_task_t* awaited_task);

void vbc_task_complete(vbc_task_t* task, const void* val, size_t size);
void vbc_task_complete_i32(vbc_task_t* task, int32_t val);
void vbc_task_complete_i64(vbc_task_t* task, int64_t val);
void vbc_task_complete_f64(vbc_task_t* task, double val);
void vbc_task_complete_str(vbc_task_t* task, vbc_string_t val);
void vbc_task_complete_ptr(vbc_task_t* task, void* ptr);
void vbc_task_fail(vbc_task_t* task, vbc_exception_t ex);

vbc_task_t* vbc_task_delay(int64_t milliseconds);
vbc_task_t* vbc_task_when_all(vbc_stack_t* stack, const vbc_slice_t* tasks);

void vbc_scheduler_run(void);
void vbc_scheduler_yield(vbc_task_t* cur_task);

#ifdef __cplusplus
}
#endif

#endif /* VBC_ASYNC_H */
