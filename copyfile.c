/*
%use error;
%use stralloc_copys;
%use stralloc_cats;
%use stralloc_append;
%use buffer_init;
%use buffer_write;
%use buffer_put;
*/
#include "error.h"
#include "stralloc.h"
#include "buffer.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
static int oneread(int fd,char *buf,unsigned int len)
{
  int r;

  for (;;) {
    r = read(fd,buf,len);
    if (r == -1) if (errno == error_intr) continue;
    return r;
  }
}
static buffer bout;
static char bout_buf[8192];
static int copyfile_tmp(const char *from, const char *to)
{
  char buf[8192];
  int blocksize;
  int n;
  int rfd;
  int wfd;
  struct stat filestat;
  if((rfd = open(from,O_RDONLY))<0) return -1;
  if((wfd = open(to,O_WRONLY|O_EXCL|O_CREAT,0644))<0) { close(rfd); return -1; }
  if(fstat(wfd,&filestat)<0) { close(rfd); close(wfd); return -1; }
  if(filestat.st_blksize < sizeof(buf)) blocksize = filestat.st_blksize;
  else blocksize = sizeof(buf);
  buffer_init(&bout,buffer_unixwrite,wfd,bout_buf,blocksize);
  for (;;) {
      if((n=oneread(rfd,buf,blocksize))<0) { close(rfd); close(wfd); return -1; }
      if(n==0) break;
      if(buffer_putalign(&bout,buf,n)<0) { close(rfd); close(wfd); return -1; }
  }
  if(close(rfd)<0) { close(wfd); return -1; }
  if(buffer_flush(&bout)<0) { close(wfd); return -1; }
  if(fsync(wfd)<0) { close(wfd); return -1; }
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
