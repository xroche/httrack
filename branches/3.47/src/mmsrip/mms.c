/*
 * $RCSfile: mms.c,v $
 * $Date: 2006/01/23 20:30:43 $ - $Revision: 1.33 $
 *
 * This file is distributed as a part of MMSRIP ( MMS Ripper ).
 * Copyright (c) 2005-2006 Nicolas BENOIT
 *
 * It is highly based on the work of SDP Multimedia and Major MMS.
 * They deserve all the credits for it.
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

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#include "common.h"
#include "mms.h"
#include "error.h"


/*
 * Put 32 bits inside a command buffer.
 */
static void
mms_put_32 ( MMS_PACKET *pak,
             uint32_t value )
{
  pak->buf[pak->num_bytes  ] = value % 256;
  value = value >> 8;
  pak->buf[pak->num_bytes+1] = value % 256 ;
  value = value >> 8;
  pak->buf[pak->num_bytes+2] = value % 256 ;
  value = value >> 8;
  pak->buf[pak->num_bytes+3] = value % 256 ;

  pak->num_bytes += 4;
  return;
}


/*
 * Returns the 32 bits located at specified offset.
 */
static uint32_t
mms_get_32 ( const unsigned char *buf,
             const int offset )
{
  return ( (((uint32_t)buf[offset+0]) << 0)  |
           (((uint32_t)buf[offset+1]) << 8)  |
           (((uint32_t)buf[offset+2]) << 16) |
           (((uint32_t)buf[offset+3]) << 24) );
}


/*
 * Returns the 64 bits located at specified offset.
 */
static uint64_t
mms_get_64 ( const unsigned char *buf,
             const int offset )
{
  return ( (((uint64_t)buf[offset+0]) << 0)  |
           (((uint64_t)buf[offset+1]) << 8)  |
           (((uint64_t)buf[offset+2]) << 16) |
           (((uint64_t)buf[offset+3]) << 24) |
           (((uint64_t)buf[offset+4]) << 32) |
           (((uint64_t)buf[offset+5]) << 40) |
           (((uint64_t)buf[offset+6]) << 48) |
           (((uint64_t)buf[offset+7]) << 56) );
}


/*
 * Converts a string into a UTF-16 string.
 */
static void
mms_string_utf16 ( uint8_t *dest,
                   unsigned char *src,
                   const ssize_t len )
{
  ssize_t i;
  memset ( dest, 0, len );

  for ( i=0; i<len; ++i )
    {
      dest[i*2] = src[i];
      dest[i*2+1] = 0;
    }

  dest[i*2] = 0;
  dest[i*2+1] = 0;
  return;
}


/*
 * Checks a URL for a correct MMS protocol.
 * Returns the host name index if the protocol is valid, else it returns MMS_RET_ERROR.
 */
static int
mms_check_protocol ( const char *url )
{
  static const char *proto[] = { "mms://", "mmsu://", "mmst://", "mmsh://", NULL };
  static const int proto_len[] = { 6, 7, 7, 7, 0 };
  register const char *p = proto[0];
  int i = 0;

  while ( p != NULL )
    {
      if ( strncmp ( url, p, proto_len[i] ) == 0 )
        return proto_len[i];

      p = proto[++i];
    }

  return MMS_RET_ERROR;
}


/*
 * Prints out a command packet (debug purpose).
 */
static void
mms_print_packet ( FILE *stddebug,
                   const MMS_PACKET *pak,
                   const ssize_t length,
                   const int orig )
{
  ssize_t i;
  unsigned char c;

  fprintf ( stddebug, "\n-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n" );

  if ( orig == MMS_SERVER )
    fprintf ( stddebug, " command from server (%d bytes)\n", length );
  else
    fprintf ( stddebug, " command from client (%d bytes)\n", length );

  fprintf ( stddebug, " start sequence: 0x%08x\n", mms_get_32(pak->buf, 0) );
  fprintf ( stddebug, " command id:     0x%08x\n", mms_get_32(pak->buf, 4) );
  fprintf ( stddebug, " length:         0x%8x \n", mms_get_32(pak->buf, 8) );
  fprintf ( stddebug, " len8:           0x%8x \n", mms_get_32(pak->buf, 16) );
  fprintf ( stddebug, " sequence #:     0x%08x\n", mms_get_32(pak->buf, 20) );
  fprintf ( stddebug, " len8  (II):     0x%8x \n", mms_get_32(pak->buf, 32) );
  fprintf ( stddebug, " dir | comm:     0x%08x\n", mms_get_32(pak->buf, 36) );
  fprintf ( stddebug, " switches:       0x%08x\n", mms_get_32(pak->buf, 40) );

  fprintf ( stddebug, "\nascii contents:\n" );

  for ( i=48; i<length; i+=2 )
    {
      c = pak->buf[i];

      if ( ( c>=32 ) && ( c<=128 ) )
        fprintf ( stddebug, "%c", c );
      else
        fprintf ( stddebug, "." );
    }

  fprintf ( stddebug, "\n\npackage hexdump:\n " );

  i = 0;

  while ( 1 )
    {
      c = pak->buf[i];
      fprintf ( stddebug, "%02x", c );

      ++i;

      if ( i < length )
        {
          if ( (i % 16) == 0 )
            fprintf ( stddebug, "\n" );

          if ( (i % 2) == 0 )
            fprintf ( stddebug, " " );
        }
      else
        break;
    }

  fprintf ( stddebug, "\n\n" );
  fflush ( stddebug );
  return;
}


/*
 * Sends a command packet to the server.
 * Returns MMS_RET_SUCCESS if success, MMS_RET_ERROR either.
 */
static int
mms_send_packet ( MMS *mms,
                  const int command,
                  const uint32_t switches, 
                  const uint32_t extra,
                  const ssize_t length,
                  const uint8_t *data )
{  
  MMS_PACKET pak;
  ssize_t written_len;

  const ssize_t len8 = ( length + (length%8) ) / 8;

  pak.num_bytes = 0;

  mms_put_32 ( &pak, 0x00000001 ); /* start sequence */
  mms_put_32 ( &pak, 0xB00BFACE ); /* #-)) */
  mms_put_32 ( &pak, length + 32 );
  mms_put_32 ( &pak, 0x20534d4d ); /* protocol type "MMS " */
  mms_put_32 ( &pak, len8 + 4 );
  mms_put_32 ( &pak, mms->seq_num++ );
  mms_put_32 ( &pak, 0x0 );        /* unknown */
  mms_put_32 ( &pak, 0x0 );
  mms_put_32 ( &pak, 2+len8 );
  mms_put_32 ( &pak, 0x00030000 | command ); /* dir | command */
  mms_put_32 ( &pak, switches );
  mms_put_32 ( &pak, extra );

  memcpy ( &pak.buf[48], data, length );

  written_len = write ( mms->socket, pak.buf, length+48 );

  if ( written_len == -1 )
    {
      if ( !mms->quiet )
#ifdef HAVE_VSNPRINTF
        error ( "mms_send_packet", "write() said: %s", strerror(errno) );
#else
        error ( "mms_send_packet", "write() failed" );
#endif

      return MMS_RET_ERROR;
    }
  else if ( written_len != (length+48) )
    {
      if ( !mms->quiet )
        error ( "mms_send_packet", "did not write enough bytes" );

      return MMS_RET_ERROR;
    }

  if ( mms->stddebug != NULL )
    mms_print_packet ( mms->stddebug, &pak, length+48, MMS_CLIENT );

  return MMS_RET_SUCCESS;
}


/*
 * Reads some bytes from the socket.
 * This is blocking until we don't have the required amount of bytes.
 * Returns MMS_RET_SUCCESS if success, MMS_RET_ERROR either.
 */
static int
mms_recv_packet ( const int s,
                  MMS_PACKET *pak,
                  size_t count,
                  const int quiet )
{
  ssize_t read_len, total;
  total = 0;

  if ( MMS_BUF_SIZE < count )
    {
      if ( !quiet )
        warning ( "mms_recv_packet", "caller is too greedy" );

      count = MMS_BUF_SIZE;
    }

  while ( total < count )
    {
      read_len = read ( s, &pak->buf[total], count-total );

      if ( read_len == -1 )
        {
          if ( !quiet )
#ifdef HAVE_VSNPRINTF
            error ( "mms_recv_packet", "read() said: %s", strerror(errno) );
#else
            error ( "mms_recv_packet", "read() failed" );
#endif

          return MMS_RET_ERROR;
        }

      total += read_len;
    }

  return MMS_RET_SUCCESS;
}


/*
 * Gets server's command.
 * The first 8 bytes can be skipped setting offset to 8.
 * It writes the packet length in the provided storage.
 * Returns command ID, MMS_CMD_INVALID if error.
 */
static int
mms_recv_cmd_packet ( const int s,
                      MMS_PACKET *pak,
                      ssize_t *packet_len,
                      const int offset,
                      const int quiet )
{
  MMS_PACKET tmp;

  if ( ( offset > 8 ) || ( offset < 0 ) )
    {
      if ( !quiet )
        error ( "mms_recv_cmd_packet", "provided offset is invalid" );

      return MMS_RET_ERROR;
    }

  if ( mms_recv_packet ( s, &tmp, 12-offset, quiet ) != 0 )
    {
      if ( !quiet )
        error ( "mms_recv_cmd_packet", "unable to get packet header" );

      return MMS_RET_ERROR;
    }

  memcpy ( &pak->buf[offset], tmp.buf, 12-offset );

  if ( offset == 0 )
    {
      if ( mms_get_32(pak->buf, 4) != 0xb00bface ) /* probably a coincidence... */
        {
          if ( !quiet )
            error ( "mms_recv_cmd_packet", "answer should have been a cmd packet" );

          return MMS_RET_ERROR;
        }
    }

  memcpy ( packet_len, &pak->buf[8], 4 );
  *packet_len = mms_get_32 ( (unsigned char *) packet_len, 0 ) + 4;

  if ( ( *packet_len + 12 ) > MMS_BUF_SIZE )
    {
      if ( !quiet )
        error ( "mms_recv_cmd_packet", "incoming packet is too big for me" );

      return MMS_RET_ERROR;
    }

  if ( mms_recv_packet ( s, &tmp, *packet_len, quiet ) != 0 )
    {
      if ( !quiet )
        error ( "mms_recv_cmd_packet", "unable to get packet body" );

      return MMS_RET_ERROR;
    }

  memcpy ( &pak->buf[12], tmp.buf, *packet_len );

  return ( mms_get_32(pak->buf, 36) & 0xFFFF );
}


/*
 * Gets the header of the ASF/WMV stream/file.
 * Returns the amount of bytes received, MMS_RET_ERROR if we encounter an error.
 */
static ssize_t
mms_recv_header_packet ( MMS *mms,
                         MMS_PACKET *pak )
{
  MMS_PACKET pre_header;
  MMS_PACKET tmp;
  ssize_t header_len;
  ssize_t packet_len;

  header_len = 0;

  while ( 1 )
    {
      if ( mms_recv_packet ( mms->socket, &pre_header, 8, mms->quiet ) != 0 )
        {
          if ( !mms->quiet )
            error ( "mms_recv_header_packet", "unable to get pre-header" );

          return MMS_RET_ERROR;
        }

      if ( pre_header.buf[4] == 0x02 )
        {
          if ( mms->stddebug != NULL )
            {
              fprintf ( mms->stddebug, "\n-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n" );
              fprintf ( mms->stddebug, " getting header packet from server\n\n" );
            }

          if ( mms->stddebug != NULL )
            {
              static int i; /* static, so we don't recreate it for each call */

              for ( i=0; i<8; ++i )
                fprintf ( mms->stddebug, "pre_header.buf[%d] = 0x%02x (%d)\n", i, pre_header.buf[i], pre_header.buf[i] );
            }

          packet_len = ( pre_header.buf[7] << 8 | pre_header.buf[6] ) - 8;

          if ( mms->stddebug != NULL )
            fprintf ( mms->stddebug, "\nASF Header Packet (%d bytes)\n", packet_len );

          if ( mms_recv_packet ( mms->socket, &tmp, packet_len, mms->quiet ) != 0 )
            {
              if ( !mms->quiet )
                error ( "mms_recv_header_packet", "unable to get header" );

              return MMS_RET_ERROR;
            }

          if ( ( header_len + packet_len ) > MMS_BUF_SIZE )
            {
              if ( !mms->quiet )
                error ( "mms_recv_header_packet", "total header length is too big for me" );

              return MMS_RET_ERROR;
            }

          memcpy ( &pak->buf[header_len], tmp.buf, packet_len );

          header_len += packet_len;

          if ( mms->stddebug != NULL )
            fprintf ( mms->stddebug, "\n" );

          /* test if end of header is reached */
          if ( ( pak->buf[header_len-1] == 1) &&
               ( pak->buf[header_len-2] == 1 ) )
            return header_len;
        }
      else /* we might receive an ack or an error */
        {
          int command = mms_recv_cmd_packet ( mms->socket, &tmp, &packet_len, 8, mms->quiet );

          if ( command == -1 )
            {
              if ( !mms->quiet )
                error ( "mms_recv_header_packet", "unable to get cmd packet" );

              return MMS_RET_ERROR;
            }

          memcpy ( tmp.buf, pre_header.buf, 8 );

          if ( mms->stddebug != NULL )
            mms_print_packet ( mms->stddebug, &tmp, packet_len, MMS_SERVER );

          if ( command == MMS_CMD_PING )
            mms_send_packet ( mms, MMS_CMD_PONG, 0, 0, 0, tmp.buf );
          else if ( command != MMS_CMD_HEADER_DATA )
            {
              if ( !mms->quiet )
#ifdef HAVE_VSNPRINTF
                error ( "mms_recv_header_packet", "unknown command 0x%02x\n", command );
#else
                error ( "mms_recv_header_packet", "unknown command\n" );
#endif

              return MMS_RET_ERROR;
            }
        }
    }
}


/*
 * Interprates the header (media packet size, etc.).
 * Returns the size of a media packet (should be != 0 ).
 */
static ssize_t
mms_interp_header ( MMS *mms,
                    const uint8_t *header,
                    const ssize_t header_len )
{
  int i;
  ssize_t packet_length = 0;

  mms->expected_file_size = header_len;

  if ( mms->stddebug != NULL )
    {
      fprintf ( mms->stddebug, "\n-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n" );
      fprintf ( mms->stddebug, " header interpretation\n\n" );
    }

  /* parse header */
  mms->num_stream_ids = 0;
  i = 30;

  while ( i < header_len )
    {
      uint64_t  guid_1, guid_2, length;

      if ( mms->stddebug != NULL )
        {
          /* this is MS GUID format */
          fprintf ( mms->stddebug, "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}\n",
                                   header[i+3], header[i+2], header[i+1], header[i+0],
                                   header[i+5], header[i+4], header[i+7], header[i+6],
                                   header[i+8], header[i+9], header[i+10], header[i+11],
                                   header[i+12], header[i+13], header[i+14], header[i+15] );
        }

      guid_2 = mms_get_64 ( header, i );
      i += 8;

      guid_1 = mms_get_64 ( header, i );
      i += 8;

      length = mms_get_64 ( header, i );
      i += 8;

      if ( ( guid_1 == 0x6553200cc000e48eULL ) && ( guid_2 == 0x11cfa9478cabdca1ULL ) )
        {
          packet_length = mms_get_32 ( header, i+92-24 );

          if ( packet_length != mms_get_32(header, i+72) )
            warning ( NULL, "size of media packets is not constant, it should not happen" );

          if ( mms->stddebug != NULL )
            {
              fprintf ( mms->stddebug, "File Properties Object (%lld bytes)\n", length );
              fprintf ( mms->stddebug, " -> broadcast bit: %d\n", mms_get_32(header,i+64)&1 );
              fprintf ( mms->stddebug, " -> min packet length: %d\n", packet_length );
              fprintf ( mms->stddebug, " -> max packet length: %d\n", mms_get_32(header, i+72) );
              fprintf ( mms->stddebug, " -> number of media packets: %d\n\n", mms_get_32(header, i+32) );
            }

          if ( mms_get_32(header,i+64)&1 )
            {
              if ( ( !mms->quiet ) && ( !mms->is_live ) )
                warning ( NULL, "stream seems to be live, an error may occur" );

              mms->is_live = MMS_LIVE;
            }
        }
      else if  ( ( guid_1 == 0x6553200cc000e68eULL ) && ( guid_2 == 0x11cfa9b7b7dc0791ULL ) )
        {
          int stream_id = header[i+48] | header[i+49] << 8;

          if ( mms->num_stream_ids < 20 )
            {
              mms->stream_ids[mms->num_stream_ids] = stream_id;
              ++mms->num_stream_ids;
            }
          else
            {
              if ( !mms->quiet )
#ifdef HAVE_VSNPRINTF
                warning ( "mms_interp_header", "too many streams, stream \'%d\' skipped", stream_id );
#else
                warning ( "mms_interp_header", "too many streams, skipping.." );
#endif
            }

          if ( mms->stddebug != NULL )
            {
              fprintf ( mms->stddebug, "Stream Object (%lld bytes)\n", length );
              fprintf ( mms->stddebug, " -> stream id: %d\n\n", stream_id );
            }
        }
      else if ( ( guid_1 == 0x6cce6200aa00d9a6ULL ) && ( guid_2 == 0x11cf668e75b22636ULL ) )
        {
          mms->expected_file_size += ( length - 50 ); /* valid values are at least 50 bytes (why?) */

          if ( mms->stddebug != NULL )
            fprintf ( mms->stddebug, "Data Object (%lld bytes)\n\n", length );
        }
      else if ( mms->stddebug != NULL )
        {
          if ( ( guid_1 == 0x6cce6200aa00d9a6ULL ) && ( guid_2 == 0x11cf668e75b22630ULL ) )
            fprintf ( mms->stddebug, "Header Object (%lld bytes)\n\n", length );
          else if ( ( guid_1 == 0xcb4903c9a000f489ULL ) && ( guid_2 == 0x11cfe5b133000890ULL) )
            fprintf ( mms->stddebug, "Simple Index Object (%lld bytes)\n\n", length );
          else if ( ( guid_1 == 0xbe4903c9a0003490ULL ) && ( guid_2 == 0x11d135dad6e229d3ULL) )
            fprintf ( mms->stddebug, "Index Object (%lld bytes)\n\n", length );
          else if ( ( guid_1 == 0x6553200cc000e38eULL ) && ( guid_2 == 0x11cfa92e5fbf03b5ULL) )
            fprintf ( mms->stddebug, "Header Extension Object (%lld bytes)\n\n", length );
          else if ( ( guid_1 == 0x6cce6200aa00d9a6ULL ) && ( guid_2 == 0x11cf668e75b22633ULL) )
            fprintf ( mms->stddebug, "Content Description Object (%lld bytes)\n\n", length );
          else if ( ( guid_1 == 0x50a85ec9a000f097ULL ) && ( guid_2 == 0x11d2e307d2d0a440ULL) )
            fprintf ( mms->stddebug, "Extended Content Description Object (%lld bytes)\n\n", length );
          else if ( ( guid_1 == 0xf64803c9a000a4a3ULL ) && ( guid_2 == 0x11d0311d86d15240ULL) )
            fprintf ( mms->stddebug, "Codec List Object (%lld bytes)\n\n", length );
          else if ( ( guid_1 == 0xb2a2c9976000828dULL ) && ( guid_2 == 0x11d1468d7bf875ceULL) )
            fprintf ( mms->stddebug, "Stream Bitrate Properties Object (%lld bytes)\n\n", length );
          else
            fprintf ( mms->stddebug, "Unknown Object (%lld bytes)\n\n", length );
        }

      i += length-24;
  }

  return packet_length;
}


/*
 * Gets a media packet.
 * Returns the amount of bytes in the media packet (0 if EOF), MMS_RET_ACKED if ACKed,
 * MMS_RET_NO_AUTH if authorization failed, MMS_RET_ERROR either.
 */
static ssize_t
mms_recv_media_packet ( MMS *mms,
                        MMS_PACKET *pak )
{
  MMS_PACKET pre_header;
  ssize_t packet_len;

  if ( mms_recv_packet ( mms->socket, &pre_header, 8, mms->quiet ) != 0 )
    {
      if ( !mms->quiet )
        error ( "mms_recv_media_packet", "unable to get pre-header" );

      return MMS_RET_ERROR;
    }

  if ( pre_header.buf[4] == 0x04 )
    {
      if ( mms->stddebug != NULL )
        {
          fprintf ( mms->stddebug, "\n-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n" );
          fprintf ( mms->stddebug, " getting media packet from server\n\n" );
        }

      if ( mms->stddebug != NULL )
        {
          static int i; /* static, so we don't recreate it for each call */

          for ( i=0; i<8; i++ )
            fprintf ( mms->stddebug, "pre_header[%d] = 0x%02x (%d)\n", i, pre_header.buf[i], pre_header.buf[i] );
        }

      packet_len = (pre_header.buf[7] << 8 | pre_header.buf[6]) - 8;

      if ( mms->stddebug != NULL )
        fprintf ( mms->stddebug, "\nASF Media Packet (%d bytes)\n", packet_len );

      memset ( pak->buf, 0, mms->media_packet_len );

      if ( mms_recv_packet (mms->socket, pak, packet_len, mms->quiet) != 0 )
        {
          if ( !mms->quiet )
            error ( "mms_recv_media_packet", "unable to get media packet" );

          return MMS_RET_ERROR;
        }
    }
  else
    {
      int command = mms_recv_cmd_packet ( mms->socket, pak, &packet_len, 8, mms->quiet );

      if ( command == MMS_RET_ERROR )
        {
          if ( !mms->quiet )
            error ( "mms_recv_media_packet", "unable to get cmd packet" );

          return MMS_RET_ERROR;
        }

      memcpy ( pak->buf, pre_header.buf, 8 );

      if ( mms->stddebug != NULL )
        mms_print_packet ( mms->stddebug, pak, packet_len, MMS_SERVER );

      if ( command == MMS_CMD_PING )
        {
          mms_send_packet ( mms, MMS_CMD_PONG, 0, 0, 0, pak->buf );
          return MMS_RET_ACKED;
        }
      else if ( command == MMS_CMD_END_OF_STREAM )
        return 0;
      else if ( command == MMS_CMD_STREAM_SELECT_ACK ) /* it happens sometimes */
        return MMS_RET_ACKED;
      else if ( command == MMS_CMD_READY_TO_STREAM ) /* it always happen before the first media packet */
        {
          /* this happens on some server for some unknown reason */
          if ( mms_get_32(pak->buf, 40) == 0x80070005 )
            {
              if ( !mms->quiet )
                error ( "mms_recv_media_packet", "streaming denied (read manpage & retry later)" );

              return MMS_RET_NO_AUTH;
            }

          return MMS_RET_ACKED;
        }
      else
        {
          if ( !mms->quiet )
#ifdef HAVE_VSNPRINTF
            error ( "mms_recv_media_packet", "unknown command 0x%02x\n", command );
#else
            error ( "mms_recv_media_packet", "unknown command\n" );
#endif

          return MMS_RET_ERROR;
        }
    }

  if ( mms->stddebug != NULL )
    fprintf ( mms->stddebug, "\n" );

  return packet_len;
}


/*
 * Creates An MMS Struct
 * Returns an initialized MMS Struct if successful, NULL if an error occurs.
 * url must start with mms:// or mmst:// (mmsu:// and mmsh:// are OK too, but we won't use UDP or do MMS over HTTP)
 * trick must be MMS_TRICK_DISABLED or MMS_TRICK_ENABLED
 */
MMS *
mms_create ( const char *url,
             FILE *out,
             FILE *stddebug,
             const int trick,
             const int quiet )
{
  MMS *mms;
  register const char *sep;
  register const int host_idx = mms_check_protocol ( url );
  register const int url_len = strlen ( url );

  if ( stddebug != NULL )
    {
      fprintf ( stddebug, "\n\n********************************************************************************\n\n" );
      fprintf ( stddebug, "Url -> \'%s\'\n", url );
    }

  if ( host_idx == MMS_RET_ERROR )
    {
      if ( !quiet )
        error ( "mms_create", "bad protocol (mms:// was expected)" );

      return NULL;
    }

  sep = strchr ( &url[host_idx], '/' );

  if ( sep == NULL )
    {
      if ( !quiet )
        error ( "mms_create", "url seems to be malformed" );

      return NULL;
    }

  if ( ( mms = ( MMS * ) malloc ( sizeof(MMS) ) ) == NULL )
    {
      if ( !quiet )
        error ( "mms_create", "unable to allocate memory" );

      return NULL;
    }

  mms->host = malloc ( ( sep - &url[host_idx] ) + 1 );
  strncpy ( mms->host, &url[host_idx], (sep - &url[host_idx]) );
  mms->host[sep-&url[host_idx]] = '\0'; /* strndup */

  mms->path = strdup ( sep+1 );
  mms->out = out;
  mms->seq_num = 0;
  mms->expected_file_size = 0;
  mms->is_live = MMS_NO_LIVE;

  /* we try to guess stream type with the filename extension */
  if ( ( sep = strchr(sep,'?') ) != NULL )
    {
      if ( ( *(sep-4) == '.' ) && /* this will never segfault because */
           ( *(sep-3) == 'w' ) && /* the url is at least 6 chars long (mms://) */
           ( *(sep-2) == 'm' ) &&
           ( *(sep-1) == 'v' ) )
        mms->stream_type = MMS_WMV;
      else
        mms->stream_type = MMS_ASF;
    }
  else
    {
      if ( ( url[url_len-4] == '.' ) &&
           ( url[url_len-3] == 'w' ) &&
           ( url[url_len-2] == 'm' ) &&
           ( url[url_len-1] == 'v' ) )
        mms->stream_type = MMS_WMV;
      else
        mms->stream_type = MMS_ASF;
    }

  mms->stddebug = stddebug;
  mms->quiet = quiet;
  mms->trick = ( ((trick==MMS_TRICK_DISABLED)||(trick==MMS_TRICK_ENABLED)) ? trick : MMS_TRICK_DISABLED );

  if ( mms->stddebug != NULL )
    {
      fprintf ( mms->stddebug, "Host -> \'%s\'\nPath -> \'%s\'\n", mms->host, mms->path );
      fprintf ( mms->stddebug, "Stream type -> %s\n", (mms->stream_type==MMS_WMV)?"MMS_WMV":"MMS_ASF" );
    }

  return mms;
}


/*
 * Establishes a connection with the MMS server.
 * Returns MMS_RET_SUCCESS if success, MMS_RET_ERROR either.
 */
int
mms_connect ( MMS* mms )
{
  struct sockaddr_in  sa;
  struct hostent     *hp;

  if ( mms == NULL )
    return -1;

  if ( ( hp = gethostbyname(mms->host) ) == NULL )
    {
      if ( !mms->quiet )
        error ( "mms_connect", "unable to resolve host name" );

      return MMS_RET_ERROR;
    }

  bcopy ( (char *) hp->h_addr, (char *) &sa.sin_addr, hp->h_length );
  sa.sin_family = hp->h_addrtype;
  sa.sin_port = htons ( 1755 );

  if ( ( mms->socket = socket(hp->h_addrtype, SOCK_STREAM, 0) ) == -1 )
    {
      if ( !mms->quiet )
#ifdef HAVE_VSNPRINTF
        error ( "mms_connect", "socket() said: %s", strerror(errno) );
#else
        error ( "mms_connect", "socket() failed" );
#endif

      return  MMS_RET_ERROR;
    }

  if ( connect ( mms->socket, (struct sockaddr *) &sa, sizeof sa ) != 0 )
    {
      if ( !mms->quiet )
#ifdef HAVE_VSNPRINTF
        error ( "mms_connect", "connect() said: %s", strerror(errno) );
#else
        error ( "mms_connect", "connect failed" );
#endif

      return MMS_RET_ERROR;
    }

  return MMS_RET_SUCCESS;
}


/*
 * Handshake with the MMS server.
 * ( we don't care about server's answers )
 */
int
mms_handshake ( MMS *mms )
{
  MMS_PACKET pak;
  int cmd = MMS_CMD_PING;
  ssize_t packet_len;
  char str [ 1024 ];
  uint8_t data [ 2092 ];

  if ( mms == NULL )
    return MMS_RET_ERROR;

  /* who we are */
  memset ( data, 0, sizeof (data) );

  if ( mms->trick == MMS_TRICK_DISABLED )
    {
#ifdef HAVE_SNPRINTF
      snprintf ( str, sizeof(str), "\034\003NSPlayer/7.0.0.1956; {3300AD50-2C39-46c0-AE0A-60181587CBA}; Host: %s", mms->host );
#else
      if ( (strlen("\034\003NSPlayer/7.0.0.1956; {3300AD50-2C39-46c0-AE0A-60181587CBA}; Host: ")+strlen(mms->host)) < sizeof(str) )
        sprintf ( str, "\034\003NSPlayer/7.0.0.1956; {3300AD50-2C39-46c0-AE0A-60181587CBA}; Host: %s", mms->host );
      else
        {
          error ( "mms_handshake", "host name is too long" );
          return MMS_RET_ERROR;
        }
#endif
    }
  else
    strcpy ( str, "\034\003NSPlayer/4.1.0.3928; {3300AD50-2C39-46c0-AE0A-60181587CBA}" );

  mms_string_utf16 ( data, (unsigned char *) str, strlen(str)+2 );
  mms_send_packet ( mms, MMS_CMD_HELLO, 0, 0x0004000b, strlen(str)*2 + 6, data );

  cmd = MMS_CMD_PING;
  while ( cmd == MMS_CMD_PING )
    {
      cmd = mms_recv_cmd_packet ( mms->socket, &pak, &packet_len, 0, mms->quiet );

      if ( cmd == MMS_CMD_PING )
        mms_send_packet ( mms, MMS_CMD_PONG, 0, 0, 0, pak.buf );
    }

  if ( cmd == MMS_CMD_INVALID )
    {
      if ( !mms->quiet )
        error ( "mms_handshake", "unable to get cmd packet" );

      return MMS_RET_ERROR;
    }

  if ( mms->stddebug != NULL )
    mms_print_packet ( mms->stddebug, &pak, packet_len, MMS_SERVER );

  /* transport protocol selection */
  memset ( &data, 0, sizeof(data) );
  mms_string_utf16 ( &data[8], (unsigned char *) "\002\000\\\\192.168.0.129\\TCP\\1037\0000", 28 );
  mms_send_packet ( mms, MMS_CMD_PROTOCOL_SELECT, 0, 0, 28*2+8, data );

  cmd = MMS_CMD_PING;
  while ( cmd == MMS_CMD_PING )
    {
      cmd = mms_recv_cmd_packet ( mms->socket, &pak, &packet_len, 0, mms->quiet );

      if ( cmd == MMS_CMD_PING )
        mms_send_packet ( mms, MMS_CMD_PONG, 0, 0, 0, pak.buf );
    }

  if ( cmd == MMS_CMD_INVALID )
    {
      if ( !mms->quiet )
        error ( "mms_handshake", "unable to get cmd packet" );

      return MMS_RET_ERROR;
    }

  if ( mms->stddebug != NULL )
    mms_print_packet ( mms->stddebug, &pak, packet_len, MMS_SERVER );

  /* requested file */
  mms_string_utf16 ( &data[8], (unsigned char *) mms->path, strlen(mms->path) );
  memset ( data, 0, 8 );
  mms_send_packet ( mms, MMS_CMD_FILE_REQUEST, 0, 0, strlen(mms->path)*2+12, data );

  cmd = MMS_CMD_PING;
  while ( cmd == MMS_CMD_PING )
    {
      cmd = mms_recv_cmd_packet ( mms->socket, &pak, &packet_len, 0, mms->quiet );

      if ( cmd == MMS_CMD_PING )
        mms_send_packet ( mms, MMS_CMD_PONG, 0, 0, 0, pak.buf );
    }

  if ( cmd == MMS_CMD_INVALID )
    {
      if ( !mms->quiet )
        error ( "mms_handshake", "unable to get cmd packet" );

      return MMS_RET_ERROR;
    }

  if ( cmd == MMS_CMD_STREAM_INFOS )
    {
      if ( mms_get_32 ( pak.buf, 48 ) == -1 )
        {
          if ( !mms->quiet )
            error ( "mms_handshake", "stream infos are not available" );

          return MMS_RET_ERROR;
        }

      if ( ( mms_get_32(pak.buf,72) == 0 ) || ( mms_get_32(pak.buf,72) == 0xFFFFFFFF ) )
        {
          mms->is_live = MMS_LIVE;

          if ( !mms->quiet )
            warning ( NULL, "stream seems to be live, an error may occur" );
        }
    }

  if ( mms->stddebug != NULL )
    mms_print_packet ( mms->stddebug, &pak, packet_len, MMS_SERVER );

  return MMS_RET_SUCCESS;
}


/*
 * Gets ASF/WMV stream/file header
 */
ssize_t
mms_write_stream_header ( MMS *mms )
{
  MMS_PACKET pak;
  ssize_t len;

  if ( mms == NULL )
    return MMS_RET_ERROR;

  /* give me the stream header please... */
  memset ( pak.buf, 0, 40 );
  pak.buf[32] = 2;
  mms_send_packet ( mms, MMS_CMD_HEADER_REQUEST, 1, 0, 40, pak.buf );

  len = mms_recv_header_packet ( mms, &pak );

  if ( len == -1 )
    {
      if ( !mms->quiet )
        error ( "mms_get_stream_header", "unable to get stream header\n" );

      return MMS_RET_ERROR;
    }
  else if ( len == 0 )
    {
      if ( !mms->quiet )
        error ( "mms_get_stream_header", "no stream header available, file may not exist" );

      return MMS_RET_ERROR;
    }

  fwrite ( pak.buf, len, 1, mms->out );

  mms->media_packet_len = mms_interp_header ( mms, pak.buf, len );

  if ( mms->media_packet_len > MMS_BUF_SIZE )
    {
      if ( !mms->quiet )
        error ( "mms_get_stream_header", "media packet length is too big for me" );

      return MMS_RET_ERROR;
    }

  return len;
}


/*
 * Tells the server we're ready to rip the stream from the beginning (don't know what happens if it's a live)
 * Returns 0 if success, -1 either.
 */
int
mms_begin_rip ( MMS *mms )
{
  int i;
  uint8_t data [ 1024 ];
  MMS_PACKET pak;
  ssize_t packet_len;
  int cmd;

  if ( mms == NULL )
    return MMS_RET_ERROR;

  /* media stream selection */
  memset ( data, 0, 40 );

  for ( i=1; i<mms->num_stream_ids; ++i )
    {
      data [ (i-1) * 6 + 2 ] = 0xFF;
      data [ (i-1) * 6 + 3 ] = 0xFF;
      data [ (i-1) * 6 + 4 ] = mms->stream_ids[i];
      data [ (i-1) * 6 + 5 ] = 0x00;
    }

  /* we add the final bytes of the stream selector, necessary for ASF streams only */
  if ( mms->stream_type == MMS_ASF )
    {
      data [ (mms->num_stream_ids-1) * 6 + 0 ] = 0x00;
      data [ (mms->num_stream_ids-1) * 6 + 1 ] = 0x00;
      data [ (mms->num_stream_ids-1) * 6 + 2 ] = 0x00;
      data [ (mms->num_stream_ids-1) * 6 + 3 ] = 0x20;
      data [ (mms->num_stream_ids-1) * 6 + 4 ] = 0xac;
      data [ (mms->num_stream_ids-1) * 6 + 5 ] = 0x40;
      data [ (mms->num_stream_ids-1) * 6 + 6 ] = 0x02;

      mms_send_packet ( mms, MMS_CMD_STREAM_SELECT, mms->num_stream_ids, 0xFFFF | mms->stream_ids[0] << 16, (mms->num_stream_ids-1)*6+2+4, data );
    }
  else
    {
      mms_send_packet ( mms, MMS_CMD_STREAM_SELECT, mms->num_stream_ids, 0xFFFF | mms->stream_ids[0] << 16, (mms->num_stream_ids-1)*6+2, data );
    }

  cmd = MMS_CMD_PING;
  while ( cmd == MMS_CMD_PING )
    {
      cmd = mms_recv_cmd_packet ( mms->socket, &pak, &packet_len, 0, mms->quiet );

      if ( cmd == MMS_CMD_PING )
        mms_send_packet ( mms, MMS_CMD_PONG, 0, 0, 0, pak.buf );
    }

  if ( cmd == MMS_CMD_INVALID )
    {
      if ( !mms->quiet )
        error ( "mms_begin_rip", "unable to get server\'s confirmation" );

      return MMS_RET_ERROR;
    }

  if ( mms->stddebug != NULL )
    mms_print_packet ( mms->stddebug, &pak, packet_len, MMS_SERVER );


  /* ready... */
  memset ( data, 0, 40 );

  for ( i=8; i<16; ++i )
    data[i] = 0xFF;

  data[20] = 0x04;

  mms_send_packet ( mms, MMS_CMD_START_PACKET, 1, 0xFFFF | mms->stream_ids[0] << 16, 24, data );

  return MMS_RET_SUCCESS;
}


/*
 * Writes a media packet.
 * Returns the amount of bytes written, 0 if EOF, MMS_RET_NO_AUTH if streaming is denied, MMS_RET_ERROR either.
 */
ssize_t
mms_write_stream_data ( MMS *mms )
{
  ssize_t len;
  MMS_PACKET pak;

  if ( mms == NULL )
    return 0;
  
  do
    {
      len = mms_recv_media_packet ( mms, &pak );
    }
  while ( len == MMS_RET_ACKED ); /* while we receive ACK packets */

  if ( len == 0 )
    return 0;
  else if ( len == -1 )
    {
      if ( !mms->quiet )
        error ( "mms_write_stream_data", "mms_recv_media_packet failed" );

      return MMS_RET_ERROR;
    }
  else if ( len == MMS_RET_NO_AUTH )
    {
      if ( !mms->quiet )
        error ( "mms_write_stream_data", "mms_recv_media_packet failed" );

      return MMS_RET_NO_AUTH;
    }

  fwrite ( pak.buf, mms->media_packet_len, 1, mms->out ); /* we might have read less than the arbitrary media packet size */

  return mms->media_packet_len;
}


/*
 * Closes the connection.
 */
void
mms_disconnect ( MMS *mms )
{
  uint8_t data [ 1024 ];

  if ( mms == NULL )
    return;

  mms_send_packet ( mms, MMS_CMD_BYE_BYE, 0, 0, 0, data );

  if ( ( shutdown(mms->socket, SHUT_RDWR) | close(mms->socket)) != 0 )
    if ( !mms->quiet )
      warning ( "mms_disconnect", "unable to close the socket properly" );

  return;
}


/*
 * Destroys An MMS Struct
 */
void
mms_destroy ( MMS *mms )
{
  if ( mms == NULL )
    return;

  if ( mms->host != NULL )
    free ( mms->host );

  if ( mms->path != NULL )
    free ( mms->path );

  free ( mms );

  return;
}
