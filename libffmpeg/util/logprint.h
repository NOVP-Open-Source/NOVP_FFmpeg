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

#ifndef __LOGPRINT_H_
#define __LOGPRINT_H_

#define LP_ERR  	1
#define LP_INFO 	2
#define LP_MSG		3
#define LP_V		4
#define LP_ALL		5
#define LP_DEBUG	6

#define LP_NONE		0
#define LP_STDERR	1
#define LP_FILE		2
#define LP_SYSLOG	4

#define LOGFILENAME     "/tmp/libxmplayer.log"
#define LOGNAME         "libxmplayer"

#define pass() printpass(__FUNCTION__,__FILE__,__LINE__);
#ifdef CRASH
#define calloc(n,s) dbg_calloc(n,s,__FUNCTION__,__FILE__,__LINE__)
#define malloc(n) dbg_malloc(n,__FUNCTION__,__FILE__,__LINE__)
#define realloc(ptr,n) dbg_realloc(ptr,n,__FUNCTION__,__FILE__,__LINE__)
#define free(ptr) dbg_free(ptr,__FUNCTION__,__FILE__,__LINE__)
void *dbg_calloc(int n, int s, char *fnc, char *fname, int line);
void *dbg_malloc(int n, char *fnc, char *fname, int line);
void *dbg_realloc(void *ptr, int n, char *fnc, char *fname, int line);
void dbg_free(void *ptr, char *fnc, char *fname, int line);
#endif

void 	   logprint(int level, char *format_str, ...);
void	   setlogfile(char *fname);
void       setlogmode(int lmode);
int	   str2logmode(char *name);
void       setloglevel(int level);
int	   str2loglevel(char *name);
void	   printpass(char *modul, char *fname, int l);

#endif

