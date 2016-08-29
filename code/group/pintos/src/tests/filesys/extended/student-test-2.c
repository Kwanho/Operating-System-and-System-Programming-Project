/* Tests that seeking past the end of a file and writing will
   properly zero out the region in between. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"


static char buf[76543];

void
test_main (void) 
{
  const char *file_name = "testfile2";
  //char zero = 0;
  int fd;

  int size = 1536;                        /* 1536 B; 3 blocks */ 


  /* create a file with size 0 */
  CHECK (create (file_name, 5000), "create \"%s\"", file_name);

  /* open the file */
  CHECK ((fd = open (file_name)) > 1, "open \"%s\"", file_name);


  msg ("seek \"%s\"", file_name);
  seek (fd, 0);


  get_stats ();

  /* write 3 blocks */
  CHECK (write (fd, buf, size) == size, "write \"%s\"", file_name);
  //CHECK (write (fd, &zero, 1) > 0, "write \"%s\"", file_name);

  get_stats ();

  msg ("close \"%s\"", file_name);
  close (fd);
}



