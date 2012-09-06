/*
 * This logging functions.
 *
 * Copyright (C) 2008 Otvos Attila oattila@onebithq.com
 *
 * This file is part of HTMLGen.
 *
 * HTMLGen is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * HTMLGen is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "config.h"
#include "util/logprint.h"

static char *logfilename = NULL;
static int logmode = LP_STDERR;
static int loglevel = LP_NONE;

void setlogfile(char *fname)
  {
  if (fname==NULL) return;
  if (logfilename!=NULL) free(logfilename);
  logfilename=strdup(fname);
  return;
  }

void setlogmode(int lmode)
  {
  logmode=lmode;
  return;
  }

int str2logmode(char *name) {
  if(!strcasecmp(name,"none"))
    return LP_NONE;
  if(!strcasecmp(name,"stderr"))
    return LP_STDERR;
  if(!strcasecmp(name,"file"))
    return LP_FILE;
  if(!strcasecmp(name,"syslog"))
    return LP_SYSLOG;
  return -1;
}

void setloglevel(int level)
  {
  if(level<1)
    return;
  loglevel=level;
  return;
  }

int str2loglevel(char *name) {
  if(!strcasecmp(name,"err"))
    return LP_ERR;
  if(!strcasecmp(name,"info"))
    return LP_INFO;
  if(!strcasecmp(name,"msg"))
    return LP_MSG;
  if(!strcasecmp(name,"v"))
    return LP_V;
  if(!strcasecmp(name,"verbose"))
    return LP_V;
  if(!strcasecmp(name,"all"))
    return LP_ALL;
  return atoi(name);
}


void logprint(int level, char *format_str, ...)
  {
  va_list ap;
  FILE *lfh;
  char buf[28];
  time_t ttime;
  struct tm *ptm;
  
  if (level>loglevel)
    return;
  if (logmode & LP_STDERR) {
    ttime=time(NULL);
    ptm=localtime(&ttime);
    strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S ",ptm);
    fprintf(stderr,"%s ",buf);
    va_start(ap,format_str);
    (void)vfprintf(stderr,format_str,ap);
//    fprintf(stderr,"\n");
    va_end(ap);
    fflush(stderr);
  }
  if (logmode & LP_FILE)
    {  
    ttime=time(NULL);
    ptm=localtime(&ttime);
    strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S ",ptm);
    lfh=stderr;
    if (logfilename==NULL)
      {
      if (NULL == (lfh=fopen(LOGFILENAME,"a+")))
        {
        fprintf(stderr,"Error:  <%i> %s (%s)",errno,strerror(errno),LOGFILENAME);
        lfh=stderr;
        }
      }
      else
      {
      if (NULL == (lfh=fopen(logfilename,"a+")))
        {
        fprintf(stderr,"Error:  <%i> %s (%s)",errno,strerror(errno),logfilename);
        lfh=stderr;
        }
      }
    va_start(ap,format_str);
    fprintf(lfh,"%s",buf);
    (void)vfprintf(lfh,format_str,ap);
//    fprintf(lfh,"\n");
    va_end(ap);
    if (lfh!=stderr) 
      {fclose(lfh); 
      if (logfilename==NULL) chmod(LOGFILENAME,S_IRUSR | S_IWUSR); else chmod(logfilename,S_IRUSR | S_IWUSR);
      }/* else fprintf(stderr,"\r"); */
    }
  if (logmode & LP_SYSLOG)
    {
    if (logfilename==NULL) openlog(LOGNAME,LOG_PID,LOG_MAIL); else openlog(logfilename,LOG_PID,LOG_MAIL);
    va_start(ap,format_str);
    vsyslog(LOG_ERR,format_str,ap);
    va_end(ap);
    closelog();
    }
  return;
  }


void printpass(char *modul, char *fname, int l) {
    fprintf(stderr,"%s: %s (%i)\n",fname,modul,l);
    fflush(stderr);
}


#undef calloc
#undef malloc
#undef realloc
#undef free

void *dbg_calloc(int n, int s, char *fnc, char *fname, int line) {
    void *ptr = calloc(n,s);
    fprintf(stderr,"%p alloc calloc(%i,%i)=%p : %s(%s:%d)\n",ptr,n,s,ptr,fname,fnc,line);
    fflush(stderr);
    return ptr;
}

void *dbg_malloc(int n, char *fnc, char *fname, int line) {
    void *ptr = malloc(n);
    fprintf(stderr,"%p alloc malloc(%i)=%p : %s(%s:%d)\n",ptr,n,ptr,fname,fnc,line);
    fflush(stderr);
    return ptr;
}

void *dbg_realloc(void *ptr, int n, char *fnc, char *fname, int line) {
    void *nptr = realloc(ptr,n);
    fprintf(stderr,"%p free realloc(%i)=%p : %s(%s:%d)\n",ptr,n,ptr,fname,fnc,line);
    fprintf(stderr,"%p alloc realloc(%i)=%p : %s(%s:%d)\n",nptr,n,ptr,fname,fnc,line);
    fflush(stderr);
    return nptr;
}

void dbg_free(void *ptr, char *fnc, char *fname, int line) {
    fprintf(stderr,"%p free free=%p : %s(%s:%d)\n",ptr,ptr,fname,fnc,line);
    fflush(stderr);
    free(ptr);
}
