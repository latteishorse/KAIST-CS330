// #include <stdbool.h>

#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

typedef int pid_t;
typedef int tid_t;

void syscall_init (void);
// void check_address(void *addr);
// void halt(void);
// void exit(int status);
// bool create (const char *file , unsigned initial_size);
// bool remove (const char *file);
// tid_t fork (const char *thread_name, struct intr_frame *f);
// int exec (const char *cmd_line);
// int wait (pid_t pid);
// int open (const char *file);
// void close (int fd);
// int filesize (int fd);
// int read (int fd, void *buffer, unsigned size);
// int write (int fd, const void *buffer, unsigned size);
// void seek (int fd, unsigned position);
// unsigned tell (int fd);
// static struct file *find_file_by_fd(int fd);
// int add_file_to_fdt(struct file *file);
// void remove_file_from_fdt(int fd);
struct lock file_rw_lock;

#endif /* userprog/syscall.h */
