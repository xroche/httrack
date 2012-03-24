/*
 * $RCSfile: error.h,v $
 * $Date: 2006/01/21 20:09:57 $ - $Revision: 1.4 $
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


#ifndef __ERROR_H__
#define __ERROR_H__

#ifdef HAVE_VSNPRINTF
#include <stdarg.h>

#define ERROR_MSG_LEN 128

void warning ( const char *, const char *, ... );
void error ( const char *, const char *, ... );
#else
void warning ( const char *, const char * );
void error ( const char *, const char * );
#endif

#endif
