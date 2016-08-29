#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"   /* Added by Group 51 */
#include "userprog/process.h"   /* Added by Group 51 */
#include "threads/pte.h"        /* Added by Group 51 */
#include "devices/input.h"      /* Added by Group 51 */

static void syscall_handler (struct intr_frame *);

void exit (int status);
int practice (int i);
int wait (tid_t tid);
tid_t exec (const char *cmd);
void halt(void);
unsigned tell (int fd);
bool create (const char* file, unsigned initial_size);
bool remove (const char* file);
void seek (int fd, unsigned position);
int filesize (int fd);
int open (const char *file);
int read (int fd, void* buffer, unsigned size);
int write (int fd, void* buffer, unsigned size);
void close (int fd);

void validate_mem (const void *uaddr);
struct lock io_lock;

void
syscall_init (void) 
{
  lock_init (&io_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t* args = ((uint32_t*) f->esp);
  validate_mem (args);

  /* void exit (int status) */
  if (args[0] == SYS_EXIT) 
    {
      validate_mem (&args[1]);
      exit (args[1]);
    } 

  /* void halt(void) */
  else if (args[0] == SYS_HALT) 
    {
      halt ();
    } 

  /* pid_t exec (conts char *cmd_line) */
  else if (args[0] == SYS_EXEC) 
    {
      tid_t rtrn = exec (args[1]);
      f->eax = rtrn;
    } 

  /* int wait (pid_t pid) */
  else if (args[0] == SYS_WAIT) 
    {
      int rtrn = wait (args[1]);
      f->eax = rtrn;
    } 

  /* bool create(const char *file, unsigned initial_size) */
  else if (args[0] == SYS_CREATE) 
    {
      bool rtrn = create (args[1], args[2]);
      f->eax = rtrn;
    } 

  /* bool remove (const char *file) */
  else if (args[0] == SYS_REMOVE) 
    {
      bool rtrn = remove (args[1]);
      f->eax = rtrn;
    } 

  /* int open(const char *file) */
  else if (args[0] == SYS_OPEN) 
    {
      f->eax = open (args[1]);
    } 

  /* int filesize(int fd) */
  else if (args[0] == SYS_FILESIZE) 
    {
      int rtrn = filesize (args[1]);
      f->eax = rtrn;
    } 

  /* int read(int fd, void* buffer, unsigned size) */
  else if (args[0] == SYS_READ) 
    {
      f->eax = read (args[1], args[2], args[3]);
    } 

  /* int write(int fd, void* buffer, unsigned size) */
  else if (args[0] == SYS_WRITE) 
    {
      f->eax = write (args[1], args[2], args[3]);
    }

  /* void seek(int fd, unsigned position) */
  else if (args[0] == SYS_SEEK) 
    {
      seek (args[1], args[2]);
    } 

  /* unsigned tell (int fd) */
  else if (args[0] == SYS_TELL) 
    {
      unsigned rtrn = tell (args[1]);
      f->eax = rtrn;
    } 

  /* void close(int fd) */
  else if (args[0] == SYS_CLOSE) 
    {
      close (args[1]);
    } 

  /* int practice(int i) */
  else if (args[0] == SYS_PRACTICE) 
    {
      int rtrn = practice (args[1]);
      f->eax = rtrn;
    }
}

void
exit (int status)
{
  printf ("%s: exit(%d)\n", &thread_current ()->name, status);
  update_wait (status);
  file_allow_write (thread_current ()->fd_table[128]);
  file_close_all (thread_current ()->fd_table);
  thread_exit ();
}

int
practice (int i)
{
  return i + 1;
}

int
wait (tid_t tid)
{
  return process_wait (tid);
}

tid_t
exec (const char *cmd_line)
{
  validate_mem (cmd_line);
  return process_execute (cmd_line);
}

void
halt ()
{
  return;
}


unsigned
tell (int fd)
{
  struct file* stored_file = thread_current ()->fd_table[fd];
  
  if (stored_file == NULL) 
    return 0;
  
  return file_tell (stored_file);
}

bool
create (const char* file, unsigned initial_size)
{
  validate_mem (file);
 
  if (file == NULL) 
    return 0;

  lock_acquire (&io_lock);

  bool output = filesys_create (file, initial_size);

  lock_release (&io_lock);

  return output;
}

bool
remove (const char* file)
{
  if (file == NULL) 
    return 0;

  lock_acquire (&io_lock);

  bool output = filesys_remove (file);

  lock_release (&io_lock);

  return output;
}

void
seek (int fd, unsigned position)
{
  struct file* stored_file = thread_current ()->fd_table[fd];

  if (stored_file == NULL) 
    return;

  file_seek (stored_file, position);
}

int 
filesize (int fd)
{
  struct file* stored_file = thread_current ()->fd_table[fd];

  if (stored_file == NULL) 
    return -1;

  return file_length (stored_file);
}

int 
open (const char *file)
{
  if (file == NULL) 
    return -1;

  validate_mem (file);

  lock_acquire (&io_lock);
  struct file* new_file = filesys_open (file);

  if (new_file == NULL) 
    {
      lock_release (&io_lock);
      return -1;
    }

  int new_fd = first_avail_fd (thread_current ()->fd_table);

  if (new_fd == -1) 
    {
      lock_release (&io_lock);
      return -1;
    }

  thread_current ()->fd_table[new_fd] = new_file;

  lock_release (&io_lock);
  return new_fd; 
}

int 
read (int fd, void* buffer, unsigned size)
{
  validate_mem (buffer);

  if (fd == 0) /* Reading from STDIN */
    {
      uint8_t* buf = (uint8_t *) buffer;
      unsigned i;

      for (i = 0; i < size; i++)
        {
          buf[i] = input_getc ();
        }
        
      return size; 
    } 

  if (fd == 1 || fd < 0 || fd > 127) /* Attempting to Read from STDOUT */  
    return -1;

  if (fd >= 2) /* Reading from open file */
    {
      lock_acquire (&io_lock);
      struct file* stored_file = thread_current ()->fd_table[fd];
      
      if (stored_file == NULL)
      {
        lock_release (&io_lock);
        return -1;
      }

      /* check if data is in cache here */

      int output = file_read (stored_file, buffer, size); 
      lock_release (&io_lock);
      return output;
    }

  /* fd is not stdin or valid fd, or mem is not valid */
  return -1;
}

int 
write (int fd, void* buffer, unsigned size)
{
  validate_mem (buffer);

  lock_acquire (&io_lock);

  if (fd == 1) /* Writing to STOUT */
    {
      putbuf (buffer, size);
      lock_release (&io_lock);
      return size; 
    } 

  if (fd <= 0 || fd > 127) /* Attempting to Write to STDIN */  
    {
      lock_release (&io_lock);
      return -1;
    }

  if (fd >= 2) /* Writing to open file */
    {
      /* get file from data structure */
      struct file* stored_file = thread_current ()->fd_table[fd];

      /* check if file is null */
      if (stored_file == NULL) 
        {
          lock_release (&io_lock);
          return -1;
        }

      lock_release (&io_lock);

      /* check if data is in cache here */
      int output = file_write (stored_file, buffer, size);
      
      return output
    }

  lock_release (&io_lock);
  return -1;
}

void
close (int fd)
{
  if (fd < 2 || fd > 127) 
    return;

  lock_acquire (&io_lock);
  struct file* stored_file = thread_current ()->fd_table[fd];

  if (stored_file == NULL) 
    {
      lock_release (&io_lock);
      return;
    }

  /* Closes the file */
  file_close (stored_file);

  /* Removes entry */
  thread_current ()->fd_table[fd] = NULL;
  lock_release (&io_lock);
}

/* Takes in a user virtual address, and performs the following memory validity checks:
   1) User Pointer is NULL
   2) User Pointer is to kernel virtual address space (aka not above PHYS_BASE)
   3) User Pointer resolves to a page that is NULL
   4) User Pointer resolves to a page in User Memory (Page should be in Kernel Mem)
   If any of these cases are true, exit with -1. Else, safely return. */
void
validate_mem (const void *uaddr)
{
  /* Cases 1 and 2 */
  if (uaddr == NULL || !(is_user_vaddr (uaddr)))
     exit (-1); 

  uint32_t* pd = thread_current ()->pagedir;

  void* kaddr = pagedir_get_page (pd, pg_round_down (uaddr));

  /* Cases 3 and 4 */
  if ((kaddr == NULL) || !(is_kernel_vaddr (kaddr)))
    exit (-1);

  return;
}
