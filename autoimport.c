/* 
%use buffer_0;
%use buffer_1;
%use buffer_2;
%use buffer_init;
%use buffer_put;
%use buffer_get;
%use buffer_read;
%use critbit0_insert;
%use critbit0_contains;
%use critbit0_allprefixed;
%use critbit0_clear;
%use copyfile;
%use checkfile;
%use checkdir;
%use getln;
%use open_read;
%use str_start;
%use stralloc_copys;
%use stralloc_cats;
%use stralloc_append;
%use strerr_sys;
%use strerr_die;
*/
#include "critbit0.h"
#include "copyfile.h"
#include "str0.h"
#include "buffer.h"
#include "stralloc.h"
#include "getln.h"
#include "strerr.h"
#include "error.h"
#include "readwrite.h"
#include "open.h"
#include "exit.h"
#include "str.h"
#include "checkfile.h"
#include "checkdir.h"

#define puts(s) buffer_putsalign(buffer_1,(s))
#define putsflush(s) buffer_putsflush(buffer_1,(s))
#define putflush() buffer_flush(buffer_1)
#define FATAL "autoimport: error: "

static critbit0_tree modules;
static critbit0_tree nextup;
static critbit0_tree objfiles;
static critbit0_tree allmodules;
static critbit0_tree executables;
static limitmalloc_pool pool = { 65536 };
static stralloc line = {0}; 
static stralloc modc = {0}; 
static stralloc repomodc = {0}; 
static stralloc autoimport_repo = {0};
static char buffer_f_space[BUFFER_INSIZE];
static buffer buffer_f;

static void cleanup()
{
    critbit0_clear(&executables,&pool);
    critbit0_clear(&modules,&pool);
    critbit0_clear(&allmodules,&pool);
    critbit0_clear(&nextup,&pool);
}
static void err_sys(const char *func)
{
    strerr_die3sys(111,FATAL,func,": ");
}
static void err_open(str0 dep)
{
    strerr_die4sys(111,FATAL,"failed to open '",dep,"': ");
}
static void err_memsoft()
{
    strerr_die2x(111,FATAL,"memory limit exceeded");
}
static void err_memhard()
{
    _exit(111);
}
static void forceclose(int fd)
{
    if(close(fd)==-1) 
    {
      strerr_die2sys(111,FATAL,"failed to close file descriptor: ");
    }
}

/* Read the file {m}.c and read all of its dependencies into the tree */
static int dependon(str0 m)
{
    int fd;
    int rc;
    int match;
    str0 newmod;

    /* No use in reading a module twice */
    if(critbit0_contains(&modules,&m)) return 0;

    /* Add the module to the tree */
    if(!critbit0_insert(&modules,&pool,&m)) err_memsoft();
    if(!critbit0_insert(&allmodules,&pool,&m)) err_memsoft();

    /* {m}.c is a new module */
    if(!stralloc_copys(&modc,m)) err_memhard();
    if(!stralloc_cats(&modc,".c")) err_memhard(); 
    if(!stralloc_0(&modc)) err_memhard();

    /* Open module source file */
    fd = open_read(modc.s);
    if(fd<0) {
        /* don't have it, check in repository */
        if(!stralloc_copys(&repomodc,autoimport_repo.s)) err_memhard();
        if(!stralloc_cats(&repomodc,"/")) err_memhard(); 
        if(!stralloc_cats(&repomodc,modc.s)) err_memhard(); 
        if(!stralloc_0(&repomodc)) err_memhard();
        fd = open_read(repomodc.s);
        if(fd<0) err_open(repomodc.s);
    }
    buffer_init(&buffer_f,buffer_unixread,fd,buffer_f_space,sizeof buffer_f_space);

    /* Read first line */
    rc = getln(&buffer_f,&line,&match,'\n');
    if(rc<0) { forceclose(fd); return 1; }

    /* Make sure first line is a comment start */
    if(!str_start(line.s,"/*")) { forceclose(fd); return 0; }

    for(;;) {
        
      /* Read next line */
      rc = getln(&buffer_f,&line,&match,'\n');
      if(rc<0 || !match) break;

      /* If line is a comment ender, we're done with this file */
      if(str_start(line.s,"*/")) break;

      /* Make sure line is of format "%use MODULE;\n" */
      if(line.len<8) continue;
      if(!str_start(line.s,"%use ")) continue; 
      if(line.s[line.len-2]!=';') continue;

 
      if(line.s[line.len-4]=='.' && line.s[line.len-3]=='o')
      {
        /* extract just the module name */
        line.s[line.len-2] = 0;
        newmod = line.s + 5;
        if(!critbit0_contains(&objfiles,&newmod))
          if(!critbit0_insert(&objfiles,&pool,&newmod)) err_memsoft();
        continue;
      }
      /* extract just the module name */
      line.s[line.len-2] = 0;
      newmod = line.s + 5;


      /* Put the name in the tree */
      if(str_start(line.s,"%use "))
      {
        if(!critbit0_contains(&nextup,&newmod))
          if(!critbit0_insert(&nextup,&pool,&newmod)) err_memsoft();
      }
    } 

    /* Done reading this file */
    forceclose(fd);
    return 0;
}

/* moredepends */
static int newcount;
static str0 moredepends_arg;
int moredepends_callback(void)
{
    ++newcount;
    if(dependon(moredepends_arg)!=0) return 0;
    str0_free(&moredepends_arg,&pool);
    return 1;
}
static int moredepends()
{
    int oldcount;
    str0 empty = "";
    oldcount = 0;
    newcount = 0;
    do {
      oldcount = newcount;
      newcount = 0;
      if(critbit0_allprefixed(&nextup, &pool, &moredepends_arg, &empty, moredepends_callback)!=1) 
          err_memsoft();
    } while(newcount!=oldcount);
    return 0;
}

/* doimport */
static stralloc filename_from = {0};
static void doimport(const char *filename_to)
{
    /* build /usr/local/autoimport/{module}.c string */
    if(!stralloc_copys(&filename_from,autoimport_repo.s)) err_memhard();
    if(!stralloc_cats(&filename_from,"/")) err_memhard();
    if(!stralloc_cats(&filename_from,filename_to)) err_memhard();
    if(!stralloc_0(&filename_from)) err_memhard();

    /* copy the file! */
    puts("autoimport: importing file: ");
    puts(filename_from.s); putsflush("\n");
    if(copyfile(filename_from.s,filename_to)<0) 
        err_sys("copyfile");
}

/* importall */
static stralloc module_dot_c = {0};
static str0 importall_arg;
static int importall_callback(void)
{
    int rc;
    /* build {module}.c string */
    if(!stralloc_copys(&module_dot_c,importall_arg)) err_memhard();
    if(!stralloc_cats(&module_dot_c,".c")) err_memhard();
    if(!stralloc_0(&module_dot_c)) err_memhard();

    /* make sure module_dot_c doesn't exist */
    rc = checkfile(module_dot_c.s);
    if(rc<0) err_sys("checkfile");
    if(rc==0) {
      /* copy /usr/local/autoimport/{module}.c to {module.c} */
      doimport(module_dot_c.s);
    } else {
      /* file with same name is already imported */
      return 1;
    }
    str0_free(&importall_arg,&pool);
    return 1;
}
static int importall()
{
    str0 empty = "";
    if(critbit0_allprefixed(&allmodules, &pool, &importall_arg, &empty, importall_callback)!=1) 
          err_memsoft();
    return 0;
}

int main(int argc, char*argv[])
{
    int i,len,rc;
    char *p;
    if(argc<=1) {
        strerr_die1x(100,"autoimport: usage: autoimport /usr/local/autoimport [files ...]");
    }
    if(checkdir(argv[1])<=0) 
        strerr_die3x(111,FATAL,argv[1]," is not a directory");
    if(!stralloc_copys(&autoimport_repo,argv[1])) err_memhard();
    if(!stralloc_0(&autoimport_repo)) err_memhard();

    for(i=2;i<argc;i++)
    {
        /* grow a new tree */
        critbit0_clear(&modules,&pool);
        critbit0_clear(&nextup,&pool);
        critbit0_clear(&objfiles,&pool);

        /* chop off .c suffix */
        p = argv[i];
        len = str0_length(&p);
        if((len > 2 
          && p[len-2] == '.' 
          && p[len-1] == 'c')) 
        {
          p[len-2] = 0;
        }
        if(!critbit0_contains(&executables,&p))
            if(!critbit0_insert(&executables,&pool,&p)) err_memsoft();

        /* add module to tree along with its dependents */
        if((rc=dependon(argv[i]))!=0) return rc;

        /* recursively get more dependencies */
        if((rc=moredepends())!=0) return rc;

    }

    /* list all modules to compile them */
    if((rc=importall())!=0) return rc;

    /* Cleanup time */
    cleanup();

    putflush();

    return 0;
}
