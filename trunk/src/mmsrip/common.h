/*
 * $RCSfile: common.h,v $
 * $Date: 2006/01/17 19:59:27 $ - $Revision: 1.11 $
 *
 * This file is distributed as a part of MMSRIP ( MMS Ripper ).
 * Copyright (c) 2005-2006 Nicolas BENOIT
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define PROGRAM_SHORT_NAME "mmsrip"
#define PROGRAM_FULL_NAME  "MMS Ripper"
#define PROGRAM_VERSION    "0.7.0-rc1"
#endif

#define PROGRAM_AUTHORS "Nicolas BENOIT <nbenoit@tuxfamily.org>"
#define PROGRAM_HELPERS "SDP Multimedia and Major MMS"


/*
 * quick struct in order to make list of streams
 */
typedef struct
{
  void *next;
  char *stream;
  char *output;
} STREAM_LIST;

#endif
