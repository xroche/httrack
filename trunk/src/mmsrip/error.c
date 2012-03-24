/*
 * $RCSfile: error.c,v $
 * $Date: 2006/01/21 20:09:57 $ - $Revision: 1.7 $
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


#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "error.h"


/*
 * Prints A Warning
 */
#ifdef HAVE_VSNPRINTF
void
warning ( const char *prod,
          const char *format,
          ... )
{
  char *message;
  va_list ap;

  if ( format == NULL )
    return;

  message = ( char * ) malloc ( ERROR_MSG_LEN );
  va_start ( ap, format );
  vsnprintf ( message, ERROR_MSG_LEN, format, ap );
  va_end ( ap );

  if ( prod != NULL )
    fprintf ( stderr, "warning in %s(): %s.\n", prod, message );
  else
    fprintf ( stderr, "warning: %s.\n", message );

  free ( message );
}
#else
void
warning ( const char *prod,
          const char *message )
{
  if ( message == NULL )
    return;

  if ( prod != NULL )
    fprintf ( stderr, "warning in %s(): %s.\n", prod, message );
  else
    fprintf ( stderr, "warning: %s.\n", message );
}
#endif


/*
 * Prints An Error Message
 */
#ifdef HAVE_VSNPRINTF
void
error ( const char *prod,
        const char *format,
        ... )
{
  char *message;
  va_list ap;

  if ( format == NULL )
    return;

  message = ( char * ) malloc ( ERROR_MSG_LEN );
  va_start ( ap, format );
  vsnprintf ( message, ERROR_MSG_LEN, format, ap );
  va_end ( ap );

  if ( prod != NULL )
    fprintf ( stderr, "error in %s(): %s.\n", prod, message );
  else
    fprintf ( stderr, "error: %s.\n", message );

  free ( message );
}
#else
void
error ( const char *prod,
        const char *message )
{
  if ( message == NULL )
    return;

  if ( prod != NULL )
    fprintf ( stderr, "error in %s(): %s.\n", prod, message );
  else
    fprintf ( stderr, "error: %s.\n", message );
}
#endif
