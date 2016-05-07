/*
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>
  Copyright (C) 2015 Alistair Mann <al+dprfs@pectw.net>
  
  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

#ifndef _FORENSICLOG_H_
#define _FORENSICLOG_H_
#include <stdio.h>

//  macro to forensiclog fields in structs.
#define forensiclog_struct(st, field, format, typecast) \
  forensiclog_msg("    " #field " = " #format "\n", typecast st->field)

FILE *forensiclog_open(void);
void forensiclog_conn (struct fuse_conn_info *conn);
void forensiclog_fi (struct fuse_file_info *fi);
void forensiclog_stat(struct stat *si);
void forensiclog_statvfs(struct statvfs *sv);
/* void forensiclog_utime(struct utimbuf *buf); */

void forensiclog_msg(const char *format, ...);
#endif
