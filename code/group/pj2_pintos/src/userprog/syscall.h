#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

/* Added by Group 51 */
void validate_mem (const void *uaddr);

#endif /* userprog/syscall.h */
