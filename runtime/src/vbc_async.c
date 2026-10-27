#include "../include/vbc_async.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <time.h>
#include <unistd.h>
#endif

static vbc_scheduler_t s_scheduler = { 0 };

static uint64_t vbc_time_now_ms(void) {
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000) + ((uint64_t)ts.tv_nsec / 1000000);
#endif
}

static void vbc_scheduler_push_ready(vbc_task_t* task) {
    if (!task) return;
    task->next = NULL;
    if (s_scheduler.ready_tail) {
        s_scheduler.ready_tail->next = task;
        s_scheduler.ready_tail = task;
    } else {
        s_scheduler.ready_head = task;
        s_scheduler.ready_tail = task;
    }
}

static vbc_task_t* vbc_scheduler_pop_ready(void) {
    if (!s_scheduler.ready_head) return NULL;
    vbc_task_t* task = s_scheduler.ready_head;
    s_scheduler.ready_head = task->next;
    if (!s_scheduler.ready_head) {
        s_scheduler.ready_tail = NULL;
    }
    task->next = NULL;
    return task;
}

#ifdef _WIN32
static void __stdcall vbc_fiber_entry(LPVOID param) {
    vbc_task_t* task = (vbc_task_t*)param;
    task->status = VBC_TASK_RUNNING;
    s_scheduler.current_task = task;

    if (task->fiber_proc) {
        task->fiber_proc(task);
    }

    if (task->status == VBC_TASK_RUNNING) {
        task->status = VBC_TASK_COMPLETED;
    }

    s_scheduler.current_task = NULL;
    SwitchToFiber(s_scheduler.main_fiber);
}
#else
static void vbc_ucontext_entry(uint32_t hi, uint32_t lo) {
    uintptr_t ptr = ((uintptr_t)hi << 32) | (uintptr_t)lo;
    vbc_task_t* task = (vbc_task_t*)ptr;
    task->status = VBC_TASK_RUNNING;
    s_scheduler.current_task = task;

    if (task->fiber_proc) {
        task->fiber_proc(task);
    }

    if (task->status == VBC_TASK_RUNNING) {
        task->status = VBC_TASK_COMPLETED;
    }

    s_scheduler.current_task = NULL;
    setcontext(&s_scheduler.main_ctx);
}
#endif

void vbc_async_init(void) {
    if (s_scheduler.initialized) return;
    s_scheduler.initialized = true;

#ifdef _WIN32
    // Convert current thread to fiber if not already converted
    s_scheduler.main_fiber = ConvertThreadToFiber(NULL);
    if (!s_scheduler.main_fiber) {
        s_scheduler.main_fiber = GetCurrentFiber();
    }
#endif
}

void vbc_async_cleanup(void) {
    if (!s_scheduler.initialized) return;

    vbc_task_t* cur = s_scheduler.all_tasks_head;
    while (cur) {
        vbc_task_t* next = cur->all_next;
        vbc_stack_free(&cur->data_stack);
        if (cur->args) {
            free(cur->args);
            cur->args = NULL;
        }
        if (cur->result_val && cur->result_val != cur->result_storage) {
            free(cur->result_val);
            cur->result_val = NULL;
        }
#ifdef _WIN32
        if (cur->fiber) {
            DeleteFiber(cur->fiber);
            cur->fiber = NULL;
        }
#else
        if (cur->stack_mem) {
            free(cur->stack_mem);
            cur->stack_mem = NULL;
        }
#endif
        free(cur);
        cur = next;
    }

    s_scheduler.all_tasks_head = NULL;
    s_scheduler.ready_head = NULL;
    s_scheduler.ready_tail = NULL;
    s_scheduler.timer_head = NULL;
    s_scheduler.initialized = false;
}

vbc_task_t* vbc_task_create(void (*fn)(void*), const void* args, size_t args_size) {
    vbc_async_init();

    vbc_task_t* task = (vbc_task_t*)calloc(1, sizeof(vbc_task_t));
    task->status = VBC_TASK_PENDING;
    task->fiber_proc = fn;
    vbc_stack_init(&task->data_stack, 32 * 1024 * 1024); // 32MB linear data stack per task

    if (args && args_size > 0) {
        task->args = malloc(args_size);
        memcpy(task->args, args, args_size);
        task->args_size = args_size;
    }

#ifdef _WIN32
    task->fiber = CreateFiber(64 * 1024, vbc_fiber_entry, task);
#else
    task->stack_size = 64 * 1024;
    task->stack_mem = malloc(task->stack_size);
    getcontext(&task->ctx);
    task->ctx.uc_stack.ss_sp = task->stack_mem;
    task->ctx.uc_stack.ss_size = task->stack_size;
    task->ctx.uc_link = &s_scheduler.main_ctx;
    uintptr_t ptr = (uintptr_t)task;
    uint32_t hi = (uint32_t)(ptr >> 32);
    uint32_t lo = (uint32_t)(ptr & 0xFFFFFFFF);
    makecontext(&task->ctx, (void(*)())vbc_ucontext_entry, 2, hi, lo);
#endif

    task->all_next = s_scheduler.all_tasks_head;
    s_scheduler.all_tasks_head = task;

    vbc_scheduler_push_ready(task);
    return task;
}

void vbc_task_complete(vbc_task_t* task, const void* val, size_t size) {
    if (!task) return;
    if (val && size > 0) {
        if (size <= sizeof(task->result_storage)) {
            memcpy(task->result_storage, val, size);
            task->result_val = task->result_storage;
        } else {
            task->result_val = malloc(size);
            memcpy(task->result_val, val, size);
        }
        task->result_size = size;
    } else {
        task->result_val = NULL;
        task->result_size = 0;
    }
    task->status = VBC_TASK_COMPLETED;
}

void vbc_task_complete_i32(vbc_task_t* task, int32_t val) {
    vbc_task_complete(task, &val, sizeof(val));
}

void vbc_task_complete_i64(vbc_task_t* task, int64_t val) {
    vbc_task_complete(task, &val, sizeof(val));
}

void vbc_task_complete_f64(vbc_task_t* task, double val) {
    vbc_task_complete(task, &val, sizeof(val));
}

void vbc_task_complete_str(vbc_task_t* task, vbc_string_t val) {
    vbc_task_complete(task, &val, sizeof(val));
}

void vbc_task_complete_ptr(vbc_task_t* task, void* ptr) {
    vbc_task_complete(task, &ptr, sizeof(ptr));
}

void vbc_task_fail(vbc_task_t* task, vbc_exception_t ex) {
    if (!task) return;
    task->exception = ex;
    task->status = VBC_TASK_FAULTED;
}

vbc_task_t* vbc_task_delay(int64_t milliseconds) {
    vbc_async_init();

    vbc_task_t* task = (vbc_task_t*)calloc(1, sizeof(vbc_task_t));
    task->status = VBC_TASK_PENDING;
    task->is_timer = true;
    if (milliseconds < 0) milliseconds = 0;
    task->wake_time_ms = vbc_time_now_ms() + (uint64_t)milliseconds;

    task->next = s_scheduler.timer_head;
    s_scheduler.timer_head = task;

    task->all_next = s_scheduler.all_tasks_head;
    s_scheduler.all_tasks_head = task;
    return task;
}

static void vbc_scheduler_step(void) {
    // 1. Process timers
    uint64_t now = vbc_time_now_ms();
    vbc_task_t* prev = NULL;
    vbc_task_t* t = s_scheduler.timer_head;
    while (t) {
        vbc_task_t* next = t->next;
        if (now >= t->wake_time_ms) {
            if (prev) {
                prev->next = next;
            } else {
                s_scheduler.timer_head = next;
            }
            t->status = VBC_TASK_COMPLETED;
            t->next = NULL;
        } else {
            prev = t;
        }
        t = next;
    }

    // 2. Process ready queue
    int count = 0;
    vbc_task_t* cur = vbc_scheduler_pop_ready();
    while (cur != NULL) {
        count++;
        if (cur->waiting_on && cur->waiting_on->status != VBC_TASK_COMPLETED && cur->waiting_on->status != VBC_TASK_FAULTED) {
            // Still waiting, requeue at back
            vbc_scheduler_push_ready(cur);
        } else {
            cur->waiting_on = NULL;
            s_scheduler.current_task = cur;
#ifdef _WIN32
            SwitchToFiber(cur->fiber);
#else
            swapcontext(&s_scheduler.main_ctx, &cur->ctx);
#endif
            s_scheduler.current_task = NULL;
            // If the task completed, we don't requeue it
            if (cur->status != VBC_TASK_COMPLETED && cur->status != VBC_TASK_FAULTED) {
                vbc_scheduler_push_ready(cur);
            }
            break; // Yield control to allow fair round-robin
        }

        // If we examined several tasks and all were waiting, break to avoid busy-spin
        if (count > 16) break;
        cur = vbc_scheduler_pop_ready();
    }

#ifdef _WIN32
    Sleep(1);
#else
    usleep(1000);
#endif
}

void* vbc_task_await(vbc_task_t* cur_task, vbc_task_t* awaited_task) {
    if (!awaited_task) return NULL;

    if (awaited_task->status == VBC_TASK_COMPLETED) {
        return awaited_task->result_val;
    }
    if (awaited_task->status == VBC_TASK_FAULTED) {
        vbc_rethrow(awaited_task->exception);
    }

    if (cur_task != NULL) {
        while (awaited_task->status != VBC_TASK_COMPLETED && awaited_task->status != VBC_TASK_FAULTED) {
            cur_task->waiting_on = awaited_task;
#ifdef _WIN32
            SwitchToFiber(s_scheduler.main_fiber);
#else
            swapcontext(&cur_task->ctx, &s_scheduler.main_ctx);
#endif
        }
        cur_task->waiting_on = NULL;
    } else {
        while (awaited_task->status != VBC_TASK_COMPLETED && awaited_task->status != VBC_TASK_FAULTED) {
            vbc_scheduler_step();
        }
    }

    if (awaited_task->status == VBC_TASK_FAULTED) {
        vbc_rethrow(awaited_task->exception);
    }
    return awaited_task->result_val;
}

void vbc_scheduler_yield(vbc_task_t* cur_task) {
    if (cur_task) {
#ifdef _WIN32
        SwitchToFiber(s_scheduler.main_fiber);
#else
        swapcontext(&cur_task->ctx, &s_scheduler.main_ctx);
#endif
    }
}

void vbc_scheduler_run(void) {
    vbc_async_init();
    s_scheduler.is_running = true;

    while (true) {
        // Check if there are active tasks or timers
        bool has_work = false;
        if (s_scheduler.ready_head != NULL || s_scheduler.timer_head != NULL) {
            has_work = true;
        }

        vbc_task_t* check = s_scheduler.all_tasks_head;
        while (check) {
            if (check->status == VBC_TASK_PENDING || check->status == VBC_TASK_RUNNING) {
                has_work = true;
                break;
            }
            check = check->all_next;
        }

        if (!has_work) break;

        vbc_scheduler_step();
    }

    s_scheduler.is_running = false;
}

typedef struct {
    vbc_slice_t tasks;
} vbc_when_all_args_t;

static void _vbc_when_all_fiber(void* arg) {
    vbc_task_t* cur_task = (vbc_task_t*)arg;
    vbc_when_all_args_t* wa = (vbc_when_all_args_t*)cur_task->args;
    vbc_task_t** task_array = (vbc_task_t**)wa->tasks.data;
    for (int64_t i = 0; i < wa->tasks.len; i++) {
        vbc_task_t* t = task_array[i];
        if (t) {
            vbc_task_await(cur_task, t);
        }
    }
    vbc_task_complete(cur_task, NULL, 0);
}

vbc_task_t* vbc_task_when_all(vbc_stack_t* stack, const vbc_slice_t* tasks) {
    (void)stack;
    if (!tasks || tasks->len == 0) {
        vbc_task_t* done = (vbc_task_t*)calloc(1, sizeof(vbc_task_t));
        done->status = VBC_TASK_COMPLETED;
        return done;
    }
    vbc_when_all_args_t args;
    args.tasks = *tasks;
    return vbc_task_create(_vbc_when_all_fiber, &args, sizeof(args));
}

