/*
 * $RCSfile: mms.h,v $
 * $Date: 2006/01/23 20:30:43 $ - $Revision: 1.17 $
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


#ifndef __MMS_H__
#define __MMS_H__

#include "common.h"

#ifdef _WIN32
typedef signed long int ssize_t;
#define bcopy(s, d, l) memcpy(d, s, l)
#include <assert.h>
#define close closesocket
#define read(soc, data, len) recv(soc, data, len, 0)
#define write(soc, data, len) send(soc, data, len, 0)
#define SHUT_RDWR SD_BOTH
#endif

#if defined(__CYGWIN__) || defined(_WIN32)
typedef unsigned char uint8_t;
#ifndef __uint32_t_defined
#define __uint32_t_defined
typedef unsigned int uint32_t;
#endif
typedef unsigned long long int uint64_t;
#else
#if defined(SOLARIS) || defined(sun)|| defined (__FreeBSD__) || defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#else
#include <stdint.h>
#endif
#endif

#define MMS_SERVER  0
#define MMS_CLIENT  1

#define MMS_NO_LIVE 0
#define MMS_LIVE    1

#define MMS_WMV     0
#define MMS_ASF     1

#define MMS_TRICK_DISABLED 0
#define MMS_TRICK_ENABLED  1

#define MMS_CMD_INVALID            -1
#define MMS_CMD_HELLO             0x01
#define MMS_CMD_PROTOCOL_SELECT   0x02
#define MMS_CMD_FILE_REQUEST      0x05
#define MMS_CMD_READY_TO_STREAM   0x05
#define MMS_CMD_STREAM_INFOS      0x06
#define MMS_CMD_START_PACKET      0x07
#define MMS_CMD_STOP_STREAM       0x09
#define MMS_CMD_BYE_BYE           0x0D
#define MMS_CMD_HEADER_DATA       0x11
#define MMS_CMD_HEADER_REQUEST    0x15
#define MMS_CMD_NET_TESTING       0x15
#define MMS_CMD_PING              0x1B
#define MMS_CMD_PONG              0x1B
#define MMS_CMD_END_OF_STREAM     0x1E
#define MMS_CMD_STREAM_SELECT_ACK 0x21
#define MMS_CMD_STREAM_SELECT     0x33

#define MMS_RET_SUCCESS           0
#define MMS_RET_ERROR            -1
#define MMS_RET_NO_AUTH          -2
#define MMS_RET_ACKED            -3


#define MMS_BUF_SIZE 102400

typedef struct
{
  uint8_t buf[MMS_BUF_SIZE];
  int     num_bytes;
} MMS_PACKET ;


typedef struct
{
  char *host;
  char *path;
  int socket;
  FILE *out;
  FILE *stddebug;
  ssize_t media_packet_len;
  uint64_t expected_file_size;
  int is_live;
  int stream_type;
  int seq_num;
  int num_stream_ids;
  int stream_ids[20];
  int quiet;
  int trick;
} MMS ;


MMS *   mms_create ( const char *, FILE *, FILE *, const int, const int );
int     mms_connect ( MMS* );
int     mms_handshake ( MMS * );
ssize_t mms_write_stream_header ( MMS * );
int     mms_begin_rip ( MMS * );
ssize_t mms_write_stream_data ( MMS * );
void    mms_disconnect ( MMS * );
void    mms_destroy ( MMS * );

#endif
