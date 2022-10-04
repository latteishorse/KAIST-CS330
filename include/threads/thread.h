#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

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
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */


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
struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. */

	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */

	/* -------------------- pjt1 - alarm_clock ------------------------- */

	int64_t wakeup_tick; 	// 깨어나야 할 tick(시각)

	/* -------------------- pjt1 - alarm_clock ------------------------- */


	/* -------------------- pjt1 - priority scheduling 3 ------------------------- */

	int initial_priority; // donation을 통해 변하기 전의 원래 priority를 저장하는 변수
	struct lock *wait_on_lock; // thread가 원하는 lock이 이미 점유 중일 때, lock의 주소를 저장
	struct list donations; // 자신에게 우선순위를 기부한 thread의 리스트
	struct list_elem donation_elem; // 내가 다른 스레드에게 우선순위를 기부했을 때 나를 기부자 리스트에 넣기 위한 표식
	
	/* -------------------- pjt1 - priority scheduling 3 ------------------------- */
	int nice;
	int recent_cpu;

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */

	// project 2. syscall
	struct list_elem child_elem;	// 자식 프로세스의 리스트에 필요한 list elem
	struct list child_list;			// 자식 프로세스의 리스트

	int exit_status;				// 종료 상태를 저장
	struct semaphore wait_sema;		// 자식 프로세스의 종료까지 부모를 대기시키기 위한 세마포어
	
	struct intr_frame parent_if; 	// _fork() 구현 때 사용, __do_fork() 함수 : tf.rsp가 시스템 콜로 인해 커널 스택을 가리킴, 커널에서 부모의 프로세스를 커널에서 복제하기 위해 저장하고 있어야 함
	struct semaphore fork_sema;		// 자식 프로세스의 fork완료까지 부모를 대기시키기 위한 세마포어
	struct semaphore free_sema;		// 자식 프로세스 종료상태를 부모가 받을때까지 종료를 대기하게 하는 세마포어
	

	// ! file related
	struct file **file_descriptor_table;	// FDT
	int fd_idx;		// fd index

    struct file *running; // 현재 실행 중인 파일
};

/* 파일 디스크립터 상수 */
#define FDT_PAGES 3
#define FDCOUNT_LIMIT FDT_PAGES * (1<<9)

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

/* -------------------- pjt1 - alarm_clock ------------------------- */
/* alram clock 구현을 위한 함수 선언 */

// 실행 중인 스레드를 슬립으로
void thread_sleep(int64_t ticks);

// 슬립 리스트에서 일어나야 할 시간이 경과한 스레드를 깨움
void thread_awake(int64_t ticks);

// 최소 틱을 가진 스레드 저장
void update_next_tick_to_awake(int64_t ticks);

// thread.c의 next_tick_to_awake 반환
// int64_t -> 64bit(8byte) 크기의 부호 있는 정수형 변수 선언
int64_t get_next_tick_to_awake(void);

/* -------------------- pjt1 - alarm_clock ------------------------- */


/* -------------------- pjt1 - priority scheduling 2 ------------------------- */
/* priority scheduling 구현을 위한 함수 선언*/

// 현재 수행중인 스레드와 가장 높은 우선순위의 스레드의 우선순위를 비교하여 스케줄링
void test_max_priority (void);

// 인자로 주어진 스레드들의 우선순위를 비교
bool cmp_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

/* -------------------- pjt1 - priority scheduling 2 ------------------------- */


/* -------------------- pjt1 - priority scheduling 3 ------------------------- */
void donate_priority (void);
void remove_with_lock (struct lock *lock);
void refresh_priority (void);
bool cmp_priority_d (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
/* -------------------- pjt1 - priority scheduling 3 ------------------------- */
void mlfqs_priority (struct thread *t);
void mlfqs_recent_cpu (struct thread *t);
void mlfqs_load_avg (void);
void mlfqs_increment (void);
void mlfqs_recalc_rc (void);
void mlfqs_recalc_p (void);

#endif /* threads/thread.h */
