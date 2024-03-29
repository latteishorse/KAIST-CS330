#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);
static void argument_stack(struct intr_frame *if_, int argv_cnt, char **argv_list);

struct thread* get_child_process (int pid);         // 해당 pid의 자식 프로세스 디스크립터를 반환 (없는 경우 NULL)

#endif /* userprog/process.h */
