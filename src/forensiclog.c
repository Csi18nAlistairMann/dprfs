/*
  Copyright (C) 2015 Alistair Mann <al+dprfs@pectw.net>
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  DPRFS is needed in case of an unwanted change to a file in the filesystem;
  the forensic log's job is to record when those changes occured and to
  what linked-list.
*/

#define  FUSE_USE_VERSION 30

#include "params.h"

#include <fuse.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "forensiclog.h"

//\/\------------
// http://stackoverflow.com/questions/5141960/get-the-current-time-in-c
// http://stackoverflow.com/questions/21905655/take-milliseconds-from-localtime-without-boost-in-c
void forensic_getSystemUTimeAndThread(char *output)
{
	struct timeval time_now;
	struct tm *time_str_tm;
	gettimeofday(&time_now, NULL);
	time_str_tm = gmtime(&time_now.tv_sec);

	sprintf(output, "%04i%02i%02i%02i%02i%02i%06i",
		time_str_tm->tm_year + 1900, time_str_tm->tm_mon + 1,
		time_str_tm->tm_mday, time_str_tm->tm_hour,
		time_str_tm->tm_min, time_str_tm->tm_sec,
		(int)time_now.tv_usec);
}

//\/\------------

FILE *forensiclog_open()
{
	FILE *forensiclogfile;

	// very first thing, open up the forensiclogfile and mark that we got in
	// here.  If we can't open the forensiclogfile, we're dead.
	forensiclogfile = fopen(LOG_DIR "/" FORENSIC_LOG, "a");
	if (forensiclogfile == NULL) {
		perror("forensiclogfile");
		exit(EXIT_FAILURE);
	}
	// set forensiclogfile to line buffering
	setvbuf(forensiclogfile, NULL, _IOLBF, 0);

	return forensiclogfile;
}

// logging after a fuse callback
void forensiclog_msg(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);

	char str[strlen(format) + 1 + 80 + 6];
	forensic_getSystemUTimeAndThread(str);
	strcat(str, " ");
	strcat(str, format);

#if RUN_AS_UNIT_TESTS
	vfprintf(stderr, str, ap);
#else
	vfprintf(DPR_DATA->forensiclogfile, str, ap);
#endif
	va_end(ap);
}

// logging in dprfs setup
void forensiclog_msg_setup(FILE * stream, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);

	char str[strlen(format) + 1 + 80];
	forensic_getSystemUTimeAndThread(str);
	strcat(str, "\t");
	strcat(str, format);

	vfprintf(stream, str, ap);
	va_end(ap);
}

// fuse context
void forensiclog_fuse_context(struct fuse_context *context)
{
	forensiclog_msg("    context:\n");
}

// struct fuse_conn_info contains information about the socket
// connection being used.  I don't actually use any of this
// information in dprfs
void forensiclog_conn(struct fuse_conn_info *conn)
{
	forensiclog_msg("    conn:\n");
}

// struct fuse_file_info keeps information about files (surprise!).
// This dumps all the information in a struct fuse_file_info.  The struct
// definition, and comments, come from /usr/include/fuse/fuse_common.h
// Duplicated here for convenience.
void forensiclog_fi(struct fuse_file_info *fi)
{
	forensiclog_msg("    fi:\n");
};

// This dumps the info from a struct stat.  The struct is defined in
// <bits/stat.h>; this is indirectly included from <fcntl.h>
void forensiclog_stat(struct stat *si)
{
	forensiclog_msg("  stat:\n");
}

void forensiclog_statvfs(struct statvfs *sv)
{
	forensiclog_msg("statvfs:\n");
}
