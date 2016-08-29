#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"   /* Added by Group 51 */
#include "userprog/process.h"   /* Added by Group 51 */
#include "threads/pte.h"        /* Added by Group 51 */
#include "devices/input.h"      /* Added by Group 51 */
#include "filesys/cache.h"      /* Added by Group 51 */
#include "filesys/directory.h"  /* Added by Group 51 */
#include <string.h>             /* Added by Group 51 */
#include "filesys/inode.h"      /* Added by Group 51 */
#include "threads/malloc.h"     /* Added by Group 51 */
#include "devices/block.h"

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


/* student testing-1 */
void reset_cache_count (void);
int get_cache_read_count (void);
int get_cache_hit_count (void);

/* student testing-2 */
void get_stats (void);


bool chdir (const char *dir);
bool mkdir (const char *dir);
bool readdir (int fd, char *name);
bool isdir (int fd);
int inumber(int fd);

void validate_mem (const void *uaddr);

void
syscall_init (void) 
{
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


  /* student testing-1 */
  else if (args[0] == SYS_TEST3) 
    {
      reset_cache_count ();
    }
  else if (args[0] == SYS_TEST4) 
    {
      int rtrn = get_cache_read_count ();
      f->eax = rtrn;
    }
  else if (args[0] == SYS_TEST5) 
    {
      int rtrn = get_cache_hit_count ();
      f->eax = rtrn;
    }

  /* student testing-2 */
  else if (args[0] == SYS_TEST6) 
    {
      get_stats ();
    }


  /* bool chdir (const char *dir) */
  else if (args[0] == SYS_CHDIR)
    {
      f->eax = (bool) chdir ((const char *)args[1]);
    }

  /* bool mkdir (const char *dir) */
  else if (args[0] == SYS_MKDIR)
    {
      f->eax = (bool) mkdir ((const char *)args[1]);
    }

  /* bool readdir (int fd, char *name) */
  else if (args[0] == SYS_READDIR)
    {
      f->eax = (bool) readdir (args[1], (char *) args[2]);
    }

  /* bool isdir (int fd) */
  else if (args[0] == SYS_ISDIR)
  {
    f->eax = isdir (args[1]);
  }

  /* int inumber(int fd) */
  else if (args[0] == SYS_INUMBER)
  {
    f->eax = inumber (args[1]);
  }
}

void
exit (int status)
{
  printf ("%s: exit(%d)\n", &thread_current ()->name, status);
  update_wait (status);
  struct fd_entry* fd_entry = thread_current ()->fd_table[128];
  struct file* exec_file = fd_entry->fd_pointer;
  file_allow_write (exec_file);
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
  struct fd_entry* fd_entry = thread_current ()->fd_table[fd];

  if (fd_entry == NULL || fd_entry->type != 0) 
    return 0;
  
  return file_tell (fd_entry->fd_pointer);
}

bool
create (const char* file, unsigned initial_size)
{
  validate_mem (file);
 
  if (file == NULL) 
    return 0;

  bool output = filesys_create (file, initial_size, 0);

  return output;
}

bool
remove (const char* file)
{
  if (file == NULL) 
    return 0;

  bool output = filesys_remove (file);

  return output;
}

void
seek (int fd, unsigned position)
{
  struct fd_entry* fd_entry = thread_current ()->fd_table[fd];

  if (fd_entry == NULL || fd_entry->type != 0)  
    return;

  file_seek (fd_entry->fd_pointer, position);
}

int 
filesize (int fd)
{
  struct fd_entry* fd_entry = thread_current ()->fd_table[fd];

  if (fd_entry == NULL || fd_entry->type != 0)
    return -1;

  return file_length (fd_entry->fd_pointer);
}

int 
open (const char *file)
{
  if (file == NULL) 
    return -1;

  validate_mem (file);

  // struct file* new_file = filesys_open (file);

  // if (new_file == NULL) 
  //   return -1;

  struct dir *dir;
  struct inode *inode = NULL;

  char part[NAME_MAX+1];
  char next_part[NAME_MAX+1];


  /* get current working directrory */
  if (file[0] == '/')
    {
      dir = dir_open_root ();
    } 
  // else if (path[0] == '.')
  // {
  //   dir = 
  // }
  // else if (path[0] == '..')
  // {
  //   dir = 
  // }
  else  // path[0] == '/' or etc
    {
      dir = dir_reopen(thread_current()->cwd);
    }
  
  /* get the file name */
  if (dir != NULL)
    {
      get_next_part(part, &file);
      while (strcmp(file, ""))
        {
          get_next_part (next_part, &file);
          dir_lookup (dir, part, &inode);
          dir_close (dir);
          dir = dir_open (inode);
          memcpy (part, next_part, strlen(next_part));
        }
      /* dir : last directory containing part */
      dir_lookup (dir, part, &inode);
      /* inode : last directory or file */
    }
  /*  */
  dir_close (dir);

  struct fd_entry* fd_entry;
  fd_entry = malloc (sizeof (struct fd_entry));

  if (inode == NULL)
    return -1;

  /* inode of a DIR */
  if (inode_isdir(inode)) 
    {
      fd_entry->fd_pointer = dir_open (inode);
      fd_entry->type = 1;
    } 
  else
    {
      fd_entry->fd_pointer = filesys_open (part);
      fd_entry->type = 0;
    }
  
  if (fd_entry->fd_pointer == NULL) 
    return -1;

  int new_fd = first_avail_fd (thread_current ()->fd_table);

  if (new_fd == -1) 
    return -1;

  thread_current ()->fd_table[new_fd] = fd_entry;

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
      struct fd_entry* fd_entry = thread_current ()->fd_table[fd];
      
      if (fd_entry == NULL || fd_entry->type != 0)
        return -1;

      /* check if data is in cache here */

      int output = file_read (fd_entry->fd_pointer, buffer, size); 
      return output;
    }

  /* fd is not stdin or valid fd, or mem is not valid */
  return -1;
}

int 
write (int fd, void* buffer, unsigned size)
{
  validate_mem (buffer);

  if (fd == 1) /* Writing to STOUT */
    {
      putbuf (buffer, size);
      return size; 
    } 

  if (fd <= 0 || fd > 127) /* Attempting to Write to STDIN */  
    return -1;

  if (fd >= 2) /* Writing to open file */
    {
      /* get file from data structure */
      struct fd_entry* fd_entry = thread_current ()->fd_table[fd];

      /* check if file is null */
      if (fd_entry == NULL || fd_entry->type != 0)
        return -1;

      /* check if data is in cache here */
      int output = file_write (fd_entry->fd_pointer, buffer, size);

      return output;
    }

  return -1;
}

void
close (int fd)
{
  if (fd < 2 || fd > 127) 
    return;

  struct fd_entry* fd_entry = thread_current ()->fd_table[fd];


  if (fd_entry == NULL || fd_entry->type != 0)
    return;

  /* Closes the file */
  if (fd_entry->type){
    dir_close(fd_entry->fd_pointer);
  } else{
    /* Closes the file */
    file_close (fd_entry->fd_pointer);
  }

  /* Removes entry */
  thread_current()->cwd = dir_open_root();
  free (thread_current ()->fd_table[fd]);
  thread_current ()->fd_table[fd] = NULL;
}


/* student testing-1 */
void
reset_cache_count ()
{
  reset_cache_cnt();
  return;
}

int
get_cache_read_count ()
{
  return get_cache_read_cnt ();
}

int
get_cache_hit_count ()
{
  return get_cache_hit_cnt ();
}

/* student testing-2 */
void
get_stats()
{
  block_print_stats ();
}



bool 
readdir (int fd, char *name)
{
  struct fd_entry* fd_entry = thread_current ()->fd_table[fd];
  return dir_readdir(fd_entry->fd_pointer, name);
}

bool
mkdir (const char *dir)
{

  if (dir == NULL) 
    return false;

  //lock_acquire (&io_lock);

  bool output = filesys_create (dir, BLOCK_SECTOR_SIZE, 1);

  //lock_release (&io_lock);

  return output;
}

bool
chdir (const char *dir)
{
  struct inode *inode = NULL;
  char curr_part[NAME_MAX + 1];
  char next_part[NAME_MAX + 1];
  struct dir *dir_ptr = get_dir(curr_part, next_part, dir);
  if (dir_ptr == NULL)
    return false;
  else
    {
      if (!dir_lookup(dir_ptr, dir,&inode))
        return false;

      thread_current()->cwd = dir_open(inode);
      return true;
    }
}

bool
isdir (int fd)
{
  return thread_current()->fd_table[fd]->type;
}

int 
inumber(int fd)
{
  struct fd_entry *fd_entry = thread_current()->fd_table[fd];
  
  if (fd_entry->type)
    return (int)inode_get_inumber(dir_get_inode((struct dir *) (fd_entry->fd_pointer)));
  
  else
    return (int)inode_get_inumber(file_get_inode((struct file *) (fd_entry->fd_pointer)));
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
