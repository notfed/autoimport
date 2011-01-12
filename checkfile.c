/* 
%use error;
*/
#include "checkfile.h"
#include "error.h"
#include <sys/stat.h>

/* checkfile: check to see if a file exists in current directory
   0 == file doesn't exist 
   1 == file exists
  -1 == error */
int checkfile(const char *filename)
{
  struct stat filestat;
  if(stat(filename,&filestat)==0) return 1;
  else if(errno == error_noent) return 0;
  return -1;
}

