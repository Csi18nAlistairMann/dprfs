/*
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>
  Copyright (C) 2015 Alistair Mann <al+dprfs@pectw.net>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

#ifndef _DEBUG_H_
#define _DEBUG_H_
#include <stdio.h>

//  macro to debug fields in structs.
#define debug_struct(st, field, format, typecast) \
  debug_msg("    " #field " = " #format "\n", typecast st->field)

FILE *debug_open(void);
void debug_msg(const struct dpr_state *dpr_data, const char *format, ...);
#endif
