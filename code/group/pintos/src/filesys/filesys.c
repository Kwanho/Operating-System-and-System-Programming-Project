#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

bool is_filesys_init = false;

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();

  /* Sets the global variable is the file_system is initialized. */
  is_filesys_init = true;
  thread_current()->cwd = dir_open_root ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, int is_dir) 
{
  block_sector_t inode_sector = 0;

  /* Catches names longer than NAME_MAX */
  if (strlen (name) > NAME_MAX && strchr (name, '/') == NULL)
    return false;
  
  /* Catches empty string for names. */
  if (!strcmp (name, ""))
    return false;


  /* Didn't use get_dir helper function because needed both access to
    DIR and PART in order to create the file with the proper name rather than the path.  */

  char part[NAME_MAX+1];
  char next_part[NAME_MAX+1];

  struct inode *inode = NULL;
  struct dir *dir;

  char *path = malloc (strlen (name)+1);
  
  /* Make a copy of name because get_next_part modifies name. */
  strlcpy (path, name, strlen (name)+1);

  
  /* Catches if the name is simply '/' */
  if (!strcmp (path, "/"))
    {
      dir = dir_open_root ();
      path = "";
    }

  /* Absolute path */
  if (path[0] == '/')
    dir = dir_open_root ();
  // else if (path[0] == '.')
  //   dir = 
  // else if (path[0] == '..')
  //   dir = 
  /* Relative path */
  else  // path[0] == '/' or etc
    dir = dir_reopen (thread_current ()->cwd);

  /* Tokenizes the path to find the last directory where the file/dir belongs to. */
  /* PART = name of the last file/dir */
  /* DIR = name of the directory PART belongs to */
  if (dir != NULL)
    {
      get_next_part (part, &path);
      while (strcmp (path, ""))
        {
          get_next_part (next_part, &path);
          dir_lookup (dir, part, &inode);
          dir_close (dir);
          dir = dir_open (inode);
          memcpy (part, next_part, strlen(next_part));
        }
    }


  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, part, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct inode *inode = NULL;

  struct dir *dir = thread_current()->cwd;

  dir_lookup (dir, name, &inode);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir;
  char part[NAME_MAX+1];
  char next_part[NAME_MAX+1];

  struct inode *inode = NULL;

  char *path = malloc(strlen(name)+1);
  
  strlcpy(path, name, strlen(name)+1);

  
  /* Didn't use get_dir helper function because needed both access to
    DIR and PART in order to remove the proper file rather than the path.  */
  if (!strcmp(path, "/"))
  {
    dir = dir_open_root();
    path = "";
  }

  if (path[0] == '/')
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

  
  if (dir != NULL)
  {
    get_next_part(part, &path);
    while (strcmp(path, ""))
    {
      get_next_part (next_part, &path);
      dir_lookup (dir, part, &inode);
      dir_close (dir);
      dir = dir_open (inode);
      memcpy (part, next_part, strlen(next_part));
    }
  }

  bool success = dir != NULL && dir_remove (dir, part);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}


/* Extracts a file name part from *SRCP into PART, and updates *SRCP so that the
   next call will return the next file name part. Returns 1 if successful, 0 at
   end of string, -1 for a too-long file name part. */
int
get_next_part (char part[NAME_MAX+1], char **srcp) {
  const char *src = *srcp;
  char *dst = part;
  /* Skip leading slashes.  If it’s all slashes, we’re done. */
  while (*src == '/')
    src++;
  if (*src == '\0')
    return 0;
  /* Copy up to NAME_MAX character from SRC to DST.  Add null terminator. */
  while (*src != '/' && *src != '\0') 
  {
    if (dst < part + NAME_MAX)
      *dst++ = *src;
    else
      return -1;
    src++; 
  }
  *dst = '\0';

  /* Advance source pointer. */
  *srcp = src;

  return 1;
}

/*  Helper function we hoped to use for create, open and remove but were only
  able to use in the syscall handler. */

struct dir *
get_dir (char *part, char *next_part, const char *name) 
{
  struct inode *inode = NULL;
  struct dir *dir;

  char *path = malloc(strlen(name)+1);
  
  strlcpy(path, name, strlen(name)+1);


  if (!strcmp(path, "/"))
  {
    dir = dir_open_root();
    path = "";
  }

  if (path[0] == '/')
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

  
  if (dir != NULL)
  {
    get_next_part(part, &path);
    while (strcmp(path, ""))
    {
      get_next_part (next_part, &path);
      dir_lookup (dir, part, &inode);
      dir_close (dir);
      dir = dir_open (inode);
      memcpy (part, next_part, strlen(next_part));
    }
  }
  return dir;
}
