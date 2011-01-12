/*
%use error;
%use stralloc_copys;
%use stralloc_cats;
%use stralloc_append;
*/
#include "error.h"
#include "stralloc.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
static int allwrite(int fd,const char *buf,unsigned int len)
{
  int w;

  while (len) {
    w = write(fd,buf,len);
    if (w == -1) {
      if (errno == error_intr) continue;
      return -1; /* note that some data may have been written */
    }
    if (w == 0) ; /* luser's fault */
    buf += w;
    len -= w;
  }
  return 0;
}
static int oneread(int fd,char *buf,unsigned int len)
{
  int r;

  for (;;) {
    r = read(fd,buf,len);
    if (r == -1) if (errno == error_intr) continue;
    return r;
  }
}
static int copyfile_tmp(const char *from, const char *to)
{
  char buf[8192];
  int blocksize;
  int n;
  int rfd;
  int wfd;
  struct stat filestat;
  if((rfd = open(from,O_RDONLY))<0) return -1;
  if((wfd = open(to,O_WRONLY|O_EXCL|O_CREAT,0644))<0) return -1;
  if(fstat(wfd,&filestat)<0) return -1;
  if(filestat.st_blksize < sizeof(buf)) blocksize = filestat.st_blksize;
  else blocksize = sizeof(buf);
  for (;;) {
      if((n=oneread(rfd,buf,blocksize))<0) return -1;
      if(n==0) break;
      if(allwrite(wfd,buf,n)<0) return -1;
  }
  if(close(rfd)<0) return -1;
  if(fsync(wfd)<0) return -1;
  if(close(wfd)<0) return -1;
  return 0;
}
static stralloc tmpfilename = {0};
int copyfile(const char *from, const char *to)
{
    if(!stralloc_copys(&tmpfilename,to)) return -1;
    if(!stralloc_cats(&tmpfilename,".tmp")) return -1;
    if(!stralloc_0(&tmpfilename)) return -1;
    if(copyfile_tmp(from,tmpfilename.s)<0) return -1;
    if(rename(tmpfilename.s,to)<0) return -1;
    if(!stralloc_copys(&tmpfilename,"")) return -1;
    return 0;
}
