#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd(const char *file_name);
tid_t process_fork(const char *name, struct intr_frame *if_);
int process_exec(void *f_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(struct thread *next);

struct thread *get_thread_by_tid(tid_t child_tid);

struct aux
{
    struct intr_frame *if_;
    struct thread *thread;
};

struct new_aux
{
    struct file *file;
    off_t offset;
    size_t page_read_bytes;
};
#ifdef VM
#define MAP_FAILED ((void *)NULL)
void *mmap(void *addr, size_t length, int writable, int fd, off_t offset);
void munmap(void *addr);
#endif
#endif /* userprog/process.h */
