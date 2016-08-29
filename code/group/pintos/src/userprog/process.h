#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

/* Added by Group 51 */
int first_avail_fd (struct fd_entry *fd_table[]);
void file_close_all (struct fd_entry *fd_table[]);

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
