/*
%use limitmalloc_open;
%use limitmalloc_close;
%use error;
*/

#include "error.h"
#include "limitmalloc.h"

void *limitmalloc_open_if2(limitmalloc_pool *p,int64 len)
{
  void *result = limitmalloc_open(p,len);
  if (!(1 & (int) result)) return result;
  limitmalloc_close(p,len,result);
  errno = error_inval;
  return 0;
}
