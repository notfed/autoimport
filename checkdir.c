/* 
%use error;
*/
#include "checkdir.h"
#include "error.h"
#include <sys/stat.h>

/* checkdir: check to see if a directory exists 
   0 == directory doesn't exist 
   1 == directory exists
  -1 == error */
int checkdir(const char *path)
{
  struct stat dirstat;
  if(stat(path,&dirstat)==0) {
    if(S_ISDIR(dirstat.st_mode)) return 1;
    else return 0;
  }
  else if(errno == error_noent) return 0;
  return -1;
}

