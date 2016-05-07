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
#define _POSIX_C_SOURCE 199309

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

#include "debug.h"

//\/\------------
// http://stackoverflow.com/questions/5141960/get-the-current-time-in-c
// http://stackoverflow.com/questions/21905655/take-milliseconds-from-localtime-without-boost-in-c
void debug_getSystemUTimeAndThread(char *output)
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

FILE *debug_open()
{
	FILE *debugfile;

	// very first thing, open up the debugfile and mark that we got in
	// here.  If we can't open the debugfile, we're dead.
	debugfile = fopen(LOG_DIR "/" DEBUG_LOG, "a");
	if (debugfile == NULL) {
		perror(LOG_DIR "/" DEBUG_LOG);
		exit(EXIT_FAILURE);
	}
	// set debugfile to line buffering
	setvbuf(debugfile, NULL, _IOLBF, 0);

	return debugfile;
}

// logging after a fuse callback
void debug_msg(const struct dpr_state *dpr_data, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);

	char str[strlen(format) + 1 + 80 + 6];
	debug_getSystemUTimeAndThread(str);
	strcat(str, "\t");
	strcat(str, format);

	if (dpr_data->debugfile != NULL)
		vfprintf(dpr_data->debugfile, str, ap);
	else
		vfprintf(stderr, str, ap);

	va_end(ap);
}
