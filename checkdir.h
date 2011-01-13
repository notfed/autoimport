#ifndef CHECKDIR_H
#define CHECKDIR_H
/* checkdir return codes:
   0 == directory doesn't exist 
   1 == directory exists
  -1 == error 
*/
int checkdir(const char *path);
#endif
