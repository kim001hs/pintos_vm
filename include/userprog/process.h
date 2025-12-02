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
/* lazy_load_segment에 전달할 정보 구조체 */
struct lazy_load_info
{
    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
};
#endif /* userprog/process.h */
