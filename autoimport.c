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

#define FATAL "autoimport: error: "

static critbit0_tree modules;
static critbit0_tree nextup;
static critbit0_tree objfiles;
static critbit0_tree allmodules;
static critbit0_tree executables;
static critbit0_tree headers;
static critbit0_tree allheaders;
static limitmalloc_pool pool = { 65536 };
static stralloc line = {0}; 
static stralloc modc = {0}; 
static stralloc repomodc = {0}; 
static stralloc autoimport_repo = {0};
static char buffer_f_space[BUFFER_INSIZE];
static buffer buffer_f;
static str0 empty = "";

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
#define err_memhard() _exit(111)

static void forceclose(int fd)
{
    if(close(fd)==-1) 
    {
      strerr_die2sys(111,FATAL,"failed to close file descriptor: ");
    }
}

static int autoimport_include(stralloc *line)
{
    str0 hdrname;
    if(line->len<11) return 0;
    if(!str_start(line->s,"#include \"")) return 0;
    if(line->s[line->len-2]!='"') return 0;
    line->s[line->len-2] = 0;
    hdrname = line->s+10;
    if(!critbit0_contains(&headers,&hdrname)) 
        if(!critbit0_insert(&headers,&pool,&hdrname)) err_memsoft();
    return 1;
}
static void autoimport_commentheader()
{
    int rc;
    int match;
    str0 newmod;
    for(;;) {
        
      /* Read next line */
      rc = getln(&buffer_f,&line,&match,'\n');
      if(rc<0 || !match) break;

      /* If line is a comment ender, we're done with this file */
      if(str_start(line.s,"*/")) break;

      /* Make sure line is of format "%use MODULE;\n" */
      if(line.len<8) continue;
      if(str_start(line.s,"%use ")) 
      {
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
        if(!critbit0_contains(&nextup,&newmod))
          if(!critbit0_insert(&nextup,&pool,&newmod)) err_memsoft();
      }
    } 
}

/* Read the file {m}.c and read all of its dependencies into the tree */
static int dependon(str0 m, int isheader)
{
    int fd;
    int rc;
    int match;

    /* No use in reading a module twice */
    if(isheader) {
      if(critbit0_contains(&allheaders,&m)) return 0; 
      if(!critbit0_insert(&allheaders,&pool,&m)) err_memsoft();
    } else {
      if(critbit0_contains(&modules,&m)) return 0;
      if(!critbit0_insert(&modules,&pool,&m)) err_memsoft();
      if(!critbit0_insert(&allmodules,&pool,&m)) err_memsoft();
    }

    /* {m}.c is a new module */
    if(!stralloc_copys(&modc,m)) err_memhard();
    if(!isheader) if(!stralloc_cats(&modc,".c")) err_memhard(); 
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

    /* Read next line */
    for(;;) {
      rc = getln(&buffer_f,&line,&match,'\n');
      if(rc<0 || !match) { forceclose(fd); return 0; }

      /* If #include, depend on include file */
      if(autoimport_include(&line)) continue;
      /* If comment start, parse comment header */
      if(str_start(line.s,"/*"))  {
        autoimport_commentheader();
      }
    }

    /* Done reading this file */
    forceclose(fd);
    return 0;
}

/* moredepends */
static int moredepends_newcount;
static str0 moredepends_arg;
static int moredepends_callback(void)
{
    ++moredepends_newcount;
    if(dependon(moredepends_arg,0)!=0) return 0;
    str0_free(&moredepends_arg,&pool);
    return 1;
}
static int moredepends()
{
    int oldcount;
    moredepends_newcount = 0;
    /* do normal files */
    do {
      oldcount = moredepends_newcount;
      moredepends_newcount = 0;
      if(critbit0_allprefixed(&nextup, &pool, &moredepends_arg, &empty, moredepends_callback)!=1) 
          err_memsoft();
    } while(moredepends_newcount!=oldcount);
    oldcount = 0;
    moredepends_newcount = 0;
    return 0;
}

/* moreheaders */
static int moreheaders_newcount;
static str0 moreheaders_arg;
static int moreheaders_callback(void)
{
    ++moreheaders_newcount;
    if(dependon(moreheaders_arg,1)!=0) return 0;
    str0_free(&moreheaders_arg,&pool);
    return 1;
}
static int moreheaders()
{
    int oldcount;
    str0 empty = "";
    moreheaders_newcount = 0;
    /* do header files */
    do {
      oldcount = moreheaders_newcount;
      moreheaders_newcount = 0;
      if(critbit0_allprefixed(&headers, &pool, &moreheaders_arg, &empty, moreheaders_callback)!=1) 
          err_memsoft();
    } while(moreheaders_newcount!=oldcount);
    oldcount = 0;
    moreheaders_newcount = 0;
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
    buffer_putsalign(buffer_1,"autoimport: importing file: ");
    buffer_putsalign(buffer_1,filename_from.s); 
    buffer_putsalign(buffer_1,"\n");
    buffer_flush(buffer_1);
    if(copyfile(filename_from.s,filename_to)<0) 
        strerr_die6sys(111,FATAL,"copying ",filename_from.s," to ",filename_to,": ");
}

/* importall */
static stralloc module_dot_c = {0};
static str0 importall_arg;
static str0 importall_extension;
static int importall_callback(void)
{
    int rc;
    /* build {module}.c string */
    if(!stralloc_copys(&module_dot_c,importall_arg)) err_memhard();
    if(!stralloc_cats(&module_dot_c,importall_extension)) err_memhard();
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
    importall_extension = ".c";
    if(critbit0_allprefixed(&allmodules, &pool, &importall_arg, &empty, importall_callback)!=1) 
          err_memsoft();
    importall_extension = "";
    if(critbit0_allprefixed(&allheaders, &pool, &importall_arg, &empty, importall_callback)!=1) 
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
        if((rc=dependon(argv[i],0))!=0) return rc;

        /* recursively get more dependencies */
        if((rc=moredepends())!=0) return rc;

        /* recursively get more dependencies */
        if((rc=moreheaders())!=0) return rc;

    }

    /* import all modules */
    if((rc=importall())!=0) return rc;

    /* Cleanup time */
    cleanup();

    buffer_flush(buffer_1);

    return 0;
}
