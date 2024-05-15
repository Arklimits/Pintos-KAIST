#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#define USERPROG  // Project 2: Error 방지
#define VM        // Proejct 3: Error 방지

#include <debug.h>
#include <list.h>
#include <stdint.h>

#include "threads/interrupt.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status {
    THREAD_RUNNING, /* Running thread. */
    THREAD_READY,   /* Not running but ready to run. */
    THREAD_BLOCKED, /* Waiting for an event to trigger. */
    THREAD_DYING    /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) - 1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN     0  /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX     63 /* Highest priority. */

/** #Project 1: Advanced Scheduler */
#define NICE_DEFAULT       0
#define RECENT_CPU_DEFAULT 0
#define LOAD_AVG_DEFAULT   0

/** #Project 2: System Call */
#define FDT_PAGES     3                     // test `multi-oom` 테스트용
#define FDCOUNT_LIMIT FDT_PAGES * (1 << 9)  // 엔트리가 512개 인 이유: 페이지 크기 4kb / 파일 포인터 8byte

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
typedef struct thread {
    /* Owned by thread.c. */
    tid_t tid;                 /* Thread identifier. */
    enum thread_status status; /* Thread state. */
    char name[16];             /* Name (for debugging purposes). */
    int priority;              /* Priority. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem; /* List element. */

    /** #Project 1: Alarm Clock */
    int64_t wakeup_tick; /* 활성화 틱 */

    /** #Project 1: Priority Donation */
    int original_priority;          /* 기존 Priority */
    struct lock *wait_lock;         /* 대기중인 lock */
    struct list donations;          /* Donation List. */
    struct list_elem donation_elem; /* Donation Element. */

    /** #Project 1: Advanced Scheduler */
    int niceness;              /* Niceness. */
    int recent_cpu;            /* 최근 CPU 점유 시간 */
    struct list_elem all_elem; /* 살아있는 모든 Thread 연결 */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint64_t *pml4; /* Page map level 4 */

    /** #Project 2: System Call */
    int exit_status;

    int fd_idx;              // 파일 디스크립터 인덱스
    struct file **fdt;       // 파일 디스크립터 테이블
    struct file *runn_file;  // 실행중인 파일

    struct intr_frame parent_if;  // 부모 프로세스 if
    struct list child_list;
    struct list_elem child_elem;

    struct semaphore fork_sema;  // fork가 완료될 때 signal
    struct semaphore exit_sema;  // 자식 프로세스 종료 signal
    struct semaphore wait_sema;  // exit_sema를 기다릴 때 사용
    /** ----------------------- */
#endif
#ifdef VM
    /* Table for whole virtual memory owned by thread. */
    struct supplemental_page_table spt;
#endif

    /* Owned by thread.c. */
    struct intr_frame tf; /* Information for switching */
    unsigned magic;       /* Detects stack overflow. */
} thread_t;

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

/** #Project 1: Alarm Clock 함수 */
void thread_sleep(int64_t ticks);
void thread_awake(int64_t ticks);
void update_next_tick_to_awake(int64_t ticks);
int64_t get_next_tick_to_awake(void);

/** #Project 1: Priority Scheduling 함수 */
void test_max_priority(void);
bool cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

/** #Project 1: Priority Donation 함수  */
void donate_priority(void);
void remove_with_lock(struct lock *lock);
void refresh_priority(void);

/** #Project 1: Advance Scheduler 함수 */
void mlfqs_priority(struct thread *t);
void mlfqs_recent_cpu(struct thread *t);
void mlfqs_load_avg(void);
void mlfqs_increment(void);
void mlfqs_recalc_recent_cpu(void);
void mlfqs_recalc_priority(void);

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void do_iret(struct intr_frame *tf);

#endif /* threads/thread.h */
