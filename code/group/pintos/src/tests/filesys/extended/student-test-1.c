/* Tests that seeking past the end of a file and writing will
   properly zero out the region in between. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

static char buf[76543];

void
test_main (void) 
{
  const char *file_name1 = "testfile1";
  const char *file_name2 = "testfile2";
  const char *file_name3 = "testfile3";
  const char *file_name4 = "testfile4";
  char zero = 0;
  int fd;
  int size1 = 60000;           /* more than 64 blocks */
  int size2 = 600;             /* less than 64 blocks */
  int size3 = 3000;            /* less than 64 blocks but bigger than size 2*/
  int size4 = 40000;           /* more than 64 blocks */

  
  int cache_read_count;
  int cache_hit_count;
  double hit_rate;



  CHECK (create (file_name1, size1), "create \"%s\" with size \"%i\"", file_name1, size1);
  CHECK (create (file_name2, size2), "create \"%s\" with size \"%i\"", file_name2, size2);
  CHECK (create (file_name3, size3), "create \"%s\" with size \"%i\"", file_name3, size3);
  CHECK (create (file_name4, size4), "create \"%s\" with size \"%i\"", file_name4, size4);


/////////////////////////////////////////////////////////////////////////////////////////////////////////

  msg ("******** file 2 (size : 600) ********");

  /* reset cache by reading file 1 */
  fd = open (file_name1);
  read (fd, buf, size1);
  close (fd);
  msg ("reset cache by reading file 1");




  /* reset cache counts */
  /* read file 2 */
  /* calculate hit rate. It should be 0 */
  CHECK ((fd = open (file_name2)) > 1, "open \"%s\"", file_name2);
  reset_cache_count();
  CHECK (read (fd, buf, size2) == size2, "read \"%s\"", file_name2);
  cache_hit_count = get_cache_hit_count ();
  cache_read_count = get_cache_read_count ();
  msg ("file 2 (first time) : cache hit count \"%i\"", cache_hit_count);
  msg ("file 2 (first time) : cache read count \"%i\"", cache_read_count);
  msg ("close \"%s\"", file_name2);
  close (fd);




  /* reset cache counts again */
  /* read file 2 again */
  /* calculate hit rate again. It should be some value between 0 and 1 */
  CHECK ((fd = open (file_name2)) > 1, "re-open \"%s\"", file_name2);
  reset_cache_count();
  CHECK (read (fd, buf, size2) == size2, "re-read \"%s\"", file_name2);
  cache_hit_count = get_cache_hit_count ();
  cache_read_count = get_cache_read_count ();
  msg ("file 2 (second time) : cache hit count \"%i\"", cache_hit_count);
  msg ("file 2 (second time) : cache read count \"%i\"", cache_read_count);
  msg ("close \"%s\"", file_name2);
  close (fd);


/////////////////////////////////////////////////////////////////////////////////////////////////////////

  msg ("******** file 3 (size : 3000) ********");

  /* reset cache by reading file 1 */
  fd = open (file_name1);
  read (fd, buf, size1);
  close (fd);
  msg ("reset cache by reading file 1");




  /* reset cache counts */
  /* read file 3 */
  /* calculate hit rate. It should be 0 */
  CHECK ((fd = open (file_name3)) > 1, "open \"%s\"", file_name3);
  reset_cache_count();
  CHECK (read (fd, buf, size3) == size3, "read \"%s\"", file_name3);
  cache_hit_count = get_cache_hit_count ();
  cache_read_count = get_cache_read_count ();
  msg ("file 3 (first time) : cache hit count \"%i\"", cache_hit_count);
  msg ("file 3 (first time) : cache read count \"%i\"", cache_read_count);
  msg ("close \"%s\"", file_name3);
  close (fd);




  /* reset cache counts again */
  /* read file 3 again */
  /* calculate hit rate again. It should be some value between 0 and 1 */
  CHECK ((fd = open (file_name3)) > 1, "re-open \"%s\"", file_name3);
  reset_cache_count();
  CHECK (read (fd, buf, size3) == size3, "re-read \"%s\"", file_name3);
  cache_hit_count = get_cache_hit_count ();
  cache_read_count = get_cache_read_count ();
  msg ("file 3 (second time) : cache hit count \"%i\"", cache_hit_count);
  msg ("file 3 (second time) : cache read count \"%i\"", cache_read_count);
  msg ("close \"%s\"", file_name3);
  close (fd);




/////////////////////////////////////////////////////////////////////////////////////////////////////////

  msg ("******** file 4 (size : 40000) ********");

  /* reset cache by reading file 1 */
  fd = open (file_name1);
  read (fd, buf, size1);
  close (fd);
  msg ("reset cache by reading file 1");




  /* reset cache counts */
  /* read file 4 */
  /* calculate hit rate. It should be 0 */
  CHECK ((fd = open (file_name4)) > 1, "open \"%s\"", file_name4);
  reset_cache_count();
  CHECK (read (fd, buf, size4) == size4, "read \"%s\"", file_name4);
  cache_hit_count = get_cache_hit_count ();
  cache_read_count = get_cache_read_count ();
  msg ("file 4 (first time) : cache hit count \"%i\"", cache_hit_count);
  msg ("file 4 (first time) : cache read count \"%i\"", cache_read_count);
  msg ("close \"%s\"", file_name4);
  close (fd);




  /* reset cache counts again */
  /* read file 4 again */
  /* calculate hit rate again. It should be some value between 0 and 1 */
  CHECK ((fd = open (file_name4)) > 1, "re-open \"%s\"", file_name4);
  reset_cache_count();
  CHECK (read (fd, buf, size4) == size4, "re-read \"%s\"", file_name4);
  cache_hit_count = get_cache_hit_count ();
  cache_read_count = get_cache_read_count ();
  msg ("file 4 (second time) : cache hit count \"%i\"", cache_hit_count);
  msg ("file 4 (second time) : cache read count \"%i\"", cache_read_count);
  msg ("close \"%s\"", file_name4);
  close (fd);


}

