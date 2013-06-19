/*
 * $RCSfile: main.c,v $
 * $Date: 2006/01/23 20:30:42 $ - $Revision: 1.32 $
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
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "common.h"
#include "mms.h"
#include "error.h"

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

/*
 * options
 */
#if defined(SOLARIS) || defined(sun)
static char * options = "ahvtqko:d:g:";
#else
static char * options = "-ahvtqko:d:g:";
#endif

#ifdef HAVE_GETOPT_LONG
static struct option long_options[] = {
  {"about",   0, NULL, 'a'},
  {"version", 0, NULL, 'v'},
  {"help",    0, NULL, 'h'},
  {"test",    0, NULL, 't'},
  {"quiet",   0, NULL, 'q'},
  {"trick",   0, NULL, 'k'},
  {"output",  1, NULL, 'o'},
  {"delay",   1, NULL, 'd'},
  {"debug",   1, NULL, 'g'},
  {NULL,      0, NULL,  0 }
};
#endif

/*
 * usage
 */
void
usage ( void )
{
  fprintf ( stderr, "%s (%s) version %s\n\n", PROGRAM_SHORT_NAME, PROGRAM_FULL_NAME, PROGRAM_VERSION );
  fprintf ( stderr, "usage: %s <[-oFILE] stream url> ...\n\n", PROGRAM_SHORT_NAME );

#ifdef HAVE_GETOPT_LONG
  fprintf ( stderr, "General Options:\n\t"                                                      \
                    "-a, --about\t\tshow information about %s\n\t"                              \
                    "-h, --help\t\tshow this help\n\t"                                          \
                    "-v, --version\t\tshow version number\n\n"                                  \
                    "Program Behaviour:\n\t"                                                    \
                    "-oFILE, --output=FILE\toutput to specified file (can be repeated)\n\t"     \
                    "-gFILE, --debug=FILE\toutput debug info to specified file\n\t"             \
                    "-q, --quiet\t\tquiet mode (can be repeated)\n\t"                           \
                    "-dDELAY, --delay=DELAY\tsave the stream during DELAY seconds and exit\n\t" \
                    "-k, --trick\t\tattempt to trick recalcitrant servers\n\t"                  \
                    "-t, --test\t\ttest mode (check stream availability)\n\n", PROGRAM_SHORT_NAME );
#else
  fprintf ( stderr, "General Options:\n\t"                                       \
                    "-a\tshow information about %s\n\t"                          \
                    "-h\tshow this help\n\t"                                     \
                    "-v\tshow version number\n\n"                                \
                    "Program Behaviour:\n\t"                                     \
                    "-oFILE\toutput to specified file (can be repeated)\n\t"     \
                    "-gFILE\toutput debug info to specified file\n\t"            \
                    "-q\tquiet mode (can be repeated)\n\t"                       \
                    "-dDELAY\tsave the stream during DELAY seconds and exit\n\t" \
                    "-k\tattempt to trick recalcitrant servers\n\t"              \
                    "-t\ttest mode (check stream availability)\n\n", PROGRAM_SHORT_NAME );
#endif

  return;
}


/*
 * main
 */
int
main ( int argc,
       char *argv[] )
{
  MMS *mms;
  FILE *f;
  FILE *stddebug = NULL;
  char *output_filename;
  STREAM_LIST *stream_list;
  STREAM_LIST *block;
  char c;
  int ret = MMS_RET_SUCCESS;
  int quiet = 0;
  int trick = MMS_TRICK_DISABLED;
  int retry = 0;
  int test = 0;
  int delay = 0;
  time_t end = 0;
  float speed = 0.0f;
  struct timeval speed_last_update;
  struct timeval now;
  float elapsed_time;
  ssize_t len_written;
  uint64_t total_len_written = 0;
  uint64_t old_total_len_written = 0;

  if ( ( stream_list = (STREAM_LIST *) malloc(sizeof(STREAM_LIST)) ) == NULL )
    {
      error ( "main", "early initialization failed" );
      return 1;
    }

  stream_list->next = NULL;
  stream_list->stream = NULL;
  stream_list->output = NULL;

  block = stream_list;

#ifdef HAVE_GETOPT_LONG
  while ( ( c = getopt_long(argc, argv, options, long_options, NULL) ) != -1 )
#elif defined(SOLARIS) || defined(sun)
  /* Implementation of getopt in Solaris is a bit strange, it returns -1 even if there are still args to parse... */
  while ( optind < argc )
#else
  while ( ( c = getopt(argc, argv, options) ) != -1 )
#endif
    {
#if defined(SOLARIS) || defined(sun)
      c = getopt ( argc, argv, options );
#endif
      switch (c)
        {
        case 'h':
          {
            fprintf ( stderr, "\n" );
            usage();
            return 0;
          }

        case 'v':
          {
            fprintf ( stderr, "%s version %s\n", PROGRAM_SHORT_NAME, PROGRAM_VERSION);
            return 0;
          }

        case 'a':
          {
            fprintf ( stderr, "\n" );
            fprintf ( stderr, "%s (%s) version %s\n\n", PROGRAM_SHORT_NAME, PROGRAM_FULL_NAME, PROGRAM_VERSION);
            fprintf ( stderr, "Written by %s.\n", PROGRAM_AUTHORS );
            fprintf ( stderr, "With a lot of help from %s.\n\n", PROGRAM_HELPERS );
            fprintf ( stderr, "This program is free software; you can redistribute it and/or\nmodify it under the terms "         \
                              "of the GNU General Public License\nas published by the Free Software Foundation; either version 3" \
                              ",\nor (at your option) any later version.\n\n" );
            return 0;
          }

        case 't':
          {
            test = 1;
            break;
          }

        case 'q':
          {
            quiet += 1;
            break;
          }

        case 'k':
          {
            trick = MMS_TRICK_ENABLED;
            break;
          }

        case 'o':
          {
            if ( optarg != NULL )
              {
                if ( block->stream != NULL )
                  {
                    if ( ( block->next = malloc ( sizeof(STREAM_LIST) ) ) == NULL )
                      {
                        error ( "main", "early initialization failed" );
                        ret = MMS_RET_ERROR;
                        goto clean;
                      }

                    block = block->next;
                    block->next = NULL;
                    block->stream = NULL;
                  }

                if ( block->output != NULL )
                  free ( block->output );

                block->output = (char *) strdup ( optarg );
              }

            break;
          }

        case 'g':
          {
            if ( optarg != NULL )
              {
                if ( stddebug != NULL )
                  fclose ( stddebug );

                if ( ( stddebug = fopen ( optarg, "w" ) ) == NULL )
                  {
                    if ( quiet < 2 )
#ifdef HAVE_VSNPRINTF
                      warning ( NULL, "unable to write debug info in \'%s\'", optarg );
#else
                      warning ( NULL, "unable to write debug info in specified file" );
#endif
                  }
                else
                  {
                    fprintf ( stddebug, "%s (%s) version %s\n\n", PROGRAM_SHORT_NAME, PROGRAM_FULL_NAME, PROGRAM_VERSION);
                    fprintf ( stddebug, "--> debug log begins now\n" );
                  }
              }

            break;
          }

        case 'd':
          {
            if ( optarg != NULL )
              {
                delay = atoi( optarg );

                if ( delay < 0 )
                  delay = 0;
              }

            break;
          }


#if defined(SOLARIS) || defined(sun)
        case -1:
          {
            if ( optind >= argc )
              break;
#else
        case 1:
          {
            if ( optarg != NULL )
              {
#endif
                if ( block->stream != NULL )
                  {
                    if ( ( block->next = malloc ( sizeof(STREAM_LIST) ) ) == NULL )
                      {
                        error ( "main", "early initialization failed" );
                        ret = MMS_RET_ERROR;
                        goto clean;
                      }

                    block = block->next;
                    block->next = NULL;
                    block->output = NULL;
                  }
#if defined(SOLARIS) || defined(sun)
                /* optind is not incremented when meeting something else than an option, so we do that... */
                block->stream = (char *) strdup ( argv[optind++] );
#else
                block->stream = (char *) strdup ( optarg );
              }
#endif
            break;
          }
        }
    }

  if ( stream_list->stream == NULL )
    {
      usage ( );
      ret = MMS_RET_ERROR;
      goto clean;
    }

  if ( !quiet )
    {
      fprintf ( stderr, "\n" );
      fprintf ( stderr, "%s (%s) version %s\n\n", PROGRAM_SHORT_NAME, PROGRAM_FULL_NAME, PROGRAM_VERSION );
    }

  for ( block=stream_list; block!=NULL; block=(STREAM_LIST*)block->next )
    {
      if ( block->stream == NULL )
        {
          if ( quiet < 2 )
            {
              if ( block->output == NULL )
#ifdef HAVE_VSNPRINTF
                error ( "main", "invalid invocation of %s", PROGRAM_SHORT_NAME );
#else
                error ( "main", "invalid invocation of mmsrip" );
#endif
              else
#ifdef HAVE_VSNPRINTF
                error ( "main", "output to \'%s\' is not attached to any stream", block->output );
#else
                error ( "main", "one of the -o output is not attached to any stream" );
#endif
            }

          ret = MMS_RET_ERROR;
          goto clean;
        }

      if ( block->output == NULL )
        {
          char *tmp = strchr ( block->stream, '/' );
          char *interro_ptr = strchr ( block->stream, '?' );

          if ( interro_ptr == NULL )
            output_filename = strrchr ( block->stream, '/' );
          else
            {
              do  /* we look for the last '/' before the first '?' */
                {
                  output_filename = tmp;
                  tmp = strchr ( tmp+1, '/' );
                }
              while ( ( tmp < interro_ptr ) && ( tmp != NULL ) );
            }

          if ( output_filename == NULL )
            {
              if ( quiet < 2 )
#ifdef HAVE_VSNPRINTF
                error ( "main", "malformed url: \'%s\'", block->stream );
#else
                error ( "main", "malformed url" );
#endif

              ret = MMS_RET_ERROR;
              continue;
            }

          ++output_filename;

          if ( strlen ( output_filename ) == 0 )
            {
              if ( quiet < 2 )
#ifdef HAVE_VSNPRINTF
                error ( "main", "malformed url: \'%s\'", block->stream );
#else
                error ( "main", "malformed url" );
#endif

              ret = MMS_RET_ERROR;
              continue;
            }

          block->output = (char *) strdup ( output_filename );

          /* we clean filenames that look like 'stream.asf?digest=7Q2bjXo&provider=lala' */
          if ( ( interro_ptr = strchr(block->output,'?') ) != NULL )
            *interro_ptr = '\0';
        }
    }

  if ( ret != MMS_RET_SUCCESS )
    goto clean;

  if ( delay != 0 )
    end = time(NULL) + delay;

  for ( block=stream_list; block!=NULL; block=(STREAM_LIST*)(retry?block:block->next) )
    {
      output_filename = block->output;
      retry = 0;

      if ( !test )
        {
          if ( ( f = fopen ( output_filename, "w" ) ) == NULL )
            {
              if ( quiet < 2 )
#ifdef HAVE_VSNPRINTF
                error ( "main", "unable to write in \'%s\'", output_filename );
#else
                error ( "main", "unable to write in output file" );
#endif

              ret = MMS_RET_ERROR;
              continue;
            }
        }
      else
        f = NULL;

      if ( ( mms = mms_create ( block->stream, f, stddebug, trick, test?1:(quiet>>1) ) ) == NULL )
        {
          if ( quiet < 2 )
            error ( "main", "unable to create mms struct" );

          if ( !test )
            {
              fclose ( f );
              remove ( output_filename );
            }

          ret = MMS_RET_ERROR;
          continue;
        }

      if ( !quiet )
        fprintf ( stderr, "Connecting ...\r" );

      if ( mms_connect ( mms ) != MMS_RET_SUCCESS )
        {
          if ( quiet < 2 )
            error ( "main", "unable to connect" );

          mms_destroy ( mms );

          if ( !test )
            {
              fclose ( f );
              remove ( output_filename );
            }

          ret = MMS_RET_ERROR;
          continue;
        }

      if ( !quiet )
        fprintf ( stderr, "Handshaking ...\r" );

      if ( mms_handshake ( mms ) != MMS_RET_SUCCESS )
        {
          if ( ( quiet < 2 ) && ( !test ) )
            error ( "main", "unable to handshake" );

          mms_disconnect ( mms );
          mms_destroy ( mms );

          if ( !test )
            {
              fclose ( f );
              remove ( output_filename );
            }

          if ( !quiet )
            {
              if ( !test )
                fprintf ( stderr, "Stream \'%s\' is not good.\n\n", block->stream );
              else
                fprintf ( stderr, "Stream \'%s\' is not good.\n", block->stream );
            }

          ret = MMS_RET_ERROR;
          continue;
        }

      if ( test )
        {
          if ( !quiet )
            fprintf ( stderr, "Stream \'%s\' is available.\n", block->stream );

          mms_disconnect ( mms );
          mms_destroy ( mms );
          continue;
        }

      if ( !quiet )
        fprintf ( stderr, "Getting header ...\r" );

      if ( ( len_written = mms_write_stream_header ( mms ) ) == MMS_RET_ERROR )
        {
          if ( quiet < 2 )
            error ( "main", "unable to write stream header" );

          mms_disconnect ( mms );
          mms_destroy ( mms );
          fclose ( f );
          remove ( output_filename );
          ret = MMS_RET_ERROR;
          continue;
        }

      total_len_written = len_written;

      if ( !quiet )
        fprintf ( stderr, "Rip is about to start ...\r" );

      if ( mms_begin_rip ( mms ) != MMS_RET_SUCCESS )
        {
          if ( quiet < 2 )
            error ( "main", "unable to begin the rip" );

          mms_disconnect ( mms );
          mms_destroy ( mms );
          fclose ( f );
          remove ( output_filename );
          ret = -1;
          continue;
        }

      if ( mms->is_live == MMS_LIVE )
        {
          if ( !quiet )
            {
              
              fprintf ( stderr, "                                                            \r" );
              fprintf ( stderr, "%s: %d bytes written (--.- kbps)\r", output_filename, (ssize_t)total_len_written );
              gettimeofday ( &speed_last_update, NULL );
            }

          while ( 1 )
            {
              len_written = mms_write_stream_data ( mms );

              if ( len_written == 0 )
                break;
              else if ( len_written == MMS_RET_ERROR )
                {
                  if ( quiet < 2 )
                    error ( "main", "unable to write stream data" );

                  ret = MMS_RET_ERROR;
                  break;
                }
              else if ( len_written == MMS_RET_NO_AUTH )
                {
                  if ( trick == MMS_TRICK_DISABLED )
                    {
                      /* we retry with the trick enabled */
                      if ( quiet < 2 )
                        fprintf ( stderr, "\r*** retrying with the anti-recalcitrant servers trick enabled ***\n" );

                      trick = MMS_TRICK_ENABLED;
                      retry = 1;
                    }
                  else
                    {
                      /* it's definitely not working */
                      if ( quiet < 2 )
                        error ( "main", "unable to write stream data" );

                      ret = MMS_RET_NO_AUTH;
                    }

                  break;
                }

              total_len_written += len_written;

              if ( !quiet )
                {
                  gettimeofday ( &now, NULL );

                  if ( now.tv_sec > speed_last_update.tv_sec )
                    {
                      elapsed_time = (now.tv_sec - speed_last_update.tv_sec) + ((now.tv_usec - speed_last_update.tv_usec) / 1000000.0f);

                      if ( elapsed_time >= 1.0f )
                        {
                          speed = ( ( ((float) (total_len_written-old_total_len_written)) / elapsed_time) / 1024.0f );
                          old_total_len_written = total_len_written;
                          gettimeofday ( &speed_last_update, NULL );
                        }
                    }

                  if ( speed > 0.0f )
                    {
                      if ( (speed / 1024.0f) >= 1.0f )
                        fprintf ( stderr, "%s: %d bytes written (%.1f mbps)    \r", output_filename, (ssize_t)total_len_written, speed/1024.0f );
                      else
                        fprintf ( stderr, "%s: %d bytes written (%.1f kbps)    \r", output_filename, (ssize_t)total_len_written, speed );
                    }
                  else
                    fprintf ( stderr, "%s: %d bytes written (--.- kbps)    \r", output_filename, (ssize_t)total_len_written );
                }

              fflush ( f );

              if ( delay != 0 )
                {
                  if ( end <= time(NULL) )
                    {
                      delay = -1;
                      break;
                    }
                }
            }

          /* for a live, we should rewrite the header */

          if ( ( ret == MMS_RET_SUCCESS ) && ( !quiet ) && ( !retry ) )
            fprintf ( stderr, "%s: %d bytes written              \r", output_filename, (ssize_t)total_len_written );
        }
      else
        {
          register const double coef = 100.0 / (double) mms->expected_file_size;

          if ( !quiet )
            {
              fprintf ( stderr, "                                                            \r" );
              fprintf ( stderr, "%s: %.2f%% (--.- kbps)\r", output_filename, (double) total_len_written * coef );
              gettimeofday ( &speed_last_update, NULL );
            }

          while ( 1 )
            {
              len_written = mms_write_stream_data ( mms );

              if ( len_written == 0 )
                break;
              else if ( len_written == MMS_RET_ERROR )
                {
                  if ( quiet < 2 )
                    error ( "main", "unable to write stream data" );

                  ret = MMS_RET_ERROR;
                  break;
                }
              else if ( len_written == MMS_RET_NO_AUTH )
                {
                  if ( trick == MMS_TRICK_DISABLED )
                    {
                      /* we retry with the trick enabled */
                      if ( quiet < 2 )
                        fprintf ( stderr, "\r*** retrying with the anti-recalcitrant servers trick enabled ***\n" );

                      trick = MMS_TRICK_ENABLED;
                      retry = 1;
                    }
                  else
                    {
                      /* it's definitely not working */
                      if ( quiet < 2 )
                        error ( "main", "unable to write stream data" );

                      ret = MMS_RET_NO_AUTH;
                    }

                  break;
                }

              total_len_written += len_written;

              if ( !quiet )
                {
                  gettimeofday ( &now, NULL );

                  if ( now.tv_sec > speed_last_update.tv_sec )
                    {
                      elapsed_time = (now.tv_sec - speed_last_update.tv_sec) + ((now.tv_usec - speed_last_update.tv_usec) / 1000000.0f);

                      if ( elapsed_time >= 1.0f )
                        {
                          speed = ( ( ((float) (total_len_written-old_total_len_written)) / elapsed_time) / 1024.0f );
                          old_total_len_written = total_len_written;
                          gettimeofday ( &speed_last_update, NULL );
                        }
                    }

                  if ( speed > 0.0f )
                    {
                      if ( (speed / 1024.0f) >= 1.0f )
                        fprintf ( stderr, "%s: %.2f%% (%.1f mbps)    \r", output_filename, (double) total_len_written * coef, speed/1024.0f );
                      else
                        fprintf ( stderr, "%s: %.2f%% (%.1f kbps)    \r", output_filename, (double) total_len_written * coef, speed );
                    }
                  else
                    fprintf ( stderr, "%s: %.2f%% (--.- kbps)    \r", output_filename, (double) total_len_written * coef );
                }

              fflush ( f );

              if ( delay != 0 )
                {
                  if ( end <= time(NULL) )
                    {
                      delay = -1;
                      break;
                    }
                }
            }

          if ( ( ret == MMS_RET_SUCCESS ) && ( !quiet ) && ( delay != -1 ) && ( !retry ) )
            fprintf ( stderr, "%s: 100.00%%                 \r", output_filename );
        }

      mms_disconnect ( mms );
      mms_destroy ( mms );

      fclose ( f );

      if ( delay == -1 )
        break;
    }

 clean:
  if ( !quiet )
    fprintf ( stderr, "\n" );

  if ( stddebug != NULL )
    {
      fprintf ( stddebug, "\n\n--> debug log ends now\n" );
      fclose ( stddebug );
    }

  for ( block=stream_list; block!=NULL; )
    {
      STREAM_LIST *old;
      old = block;
      block = (STREAM_LIST *) block->next;

      if ( old->stream != NULL )
        free ( old->stream );

      if ( old->output != NULL )
        free ( old->output );

      free ( old );
    }

  return (ret<0) ? (ret*-1) : ret;
}
