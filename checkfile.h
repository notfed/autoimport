#ifndef CHECKFILE_H
#define CHECKFILE_H
/* checkfile return codes:
   0 == file doesn't exist 
   1 == file exists
  -1 == error 
*/
int checkfile(const char *filename);
#endif
