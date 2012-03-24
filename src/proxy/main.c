/* ------------------------------------------------------------ */
/*
HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

Please visit our Website: http://www.httrack.com
*/

/* ------------------------------------------------------------ */
/* File: ProxyTrack, httrack cache-based proxy                  */
/* Author: Xavier Roche                                         */
/* ------------------------------------------------------------ */

/* Standard includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "htsbase.h"
#include "htsnet.h"
#include "htslib.h"
#include "store.h"
#include "proxytrack.h"

#ifndef _WIN32
#include <signal.h>
static void sig_brpipe( int code ) {
  /* ignore */
}
#endif

static int scanHostPort(const char* str, char *host, int *port) {
	char* pos = strrchr(str, ':');
	if (pos != NULL) {
		int n = (int) ( pos - str );
		if (n < 256) {
			host[0] = '\0';
			strncat(host, str, n);
			if (sscanf(pos + 1, "%d", port) == 1) {
				return 1;
			}
		}
	}
	return 0;
}

int main(int argc, char* argv[])
{
  int i;
  int ret = 0;
	int proxyPort = 0, icpPort = 0;
	char proxyAddr[256 + 1], icpAddr[256 + 1];
	PT_Indexes index;

#ifdef _WIN32
  {
    WORD   wVersionRequested;   // requested version WinSock API
    WSADATA wsadata;            // Windows Sockets API data
    int stat;
    wVersionRequested = 0x0101;
    stat = WSAStartup( wVersionRequested, &wsadata );
    if (stat != 0) {
      fprintf(stderr, "Winsock not found!\n");
      return -1;
    } else if (LOBYTE(wsadata.wVersion) != 1  && HIBYTE(wsadata.wVersion) != 1) {
      fprintf(stderr, "WINSOCK.DLL does not support version 1.1\n");
      WSACleanup();
      return -1;
    }
  }
#endif

	/* Args */
	printf("ProxyTrack %s, build proxies upon HTTrack Website Copier Archives\n", PROXYTRACK_VERSION);
	printf("Copyright (C) Xavier Roche and other contributors\n");
	printf("\n");
	printf("This program is free software; you can redistribute it and/or\n");
	printf("modify it under the terms of the GNU General Public License\n");
	printf("as published by the Free Software Foundation; either version 3\n");
	printf("of the License, or any later version.\n");
	printf("\n");
	printf("*** This version is a development release ***\n");
	printf("\n");
	if (argc < 3
		|| (
			strcmp(argv[1], "--convert") != 0
			&&
			(
				!scanHostPort(argv[1], proxyAddr, &proxyPort)
				|| !scanHostPort(argv[2], icpAddr, &icpPort)
			)
			)
		) 
	{
		fprintf(stderr, "proxy mode:\n");
		fprintf(stderr, "usage: %s <proxy-addr:proxy-port> <ICP-addr:ICP-port> [ ( <new.zip path> | <new.ndx path> | <archive.arc path> | --list <file-list> ) ..]\n", argv[0]);
		fprintf(stderr, "\texample:%s proxy:8080 localhost:3130 /home/archives/www-archive-01.zip /home/old-archives/www-archive-02.ndx\n", argv[0]);
		fprintf(stderr, "convert mode:\n");
		fprintf(stderr, "usage: %s --convert <archive-output-path> [ ( <new.zip path> | <new.ndx path> | <archive.arc path> | --list <file-list> ) ..]\n", argv[0]);
		fprintf(stderr, "\texample:%s proxy:8080 localhost:3130 /home/archives/www-archive-01.zip /home/old-archives/www-archive-02.ndx\n", argv[0]);
    return 1;
  }
	index = PT_New();
	for(i = 3 ; i < argc ; i++) {
		if (argv[i][0] == '-') {
			if (strcmp(argv[i], "--list") == 0) {
				if (i + 1 < argc) {
					char line[256 + 1];
					FILE *fp = fopen(argv[++i], "rb");
					if (fp == NULL) {
						fprintf(stderr, "error: could not process list %s\n", argv[i]);
						exit(1);
					}
					while(linput(fp, line, 256)) {
						int itemsAdded = PT_AddIndex(index, line);
						if (itemsAdded > 0) {
							fprintf(stderr, "processed: %s (%d items added)\n", line, itemsAdded);
						} else if (itemsAdded == 0) {
							fprintf(stderr, "processed: %s (no items added)\n", line);
						} else {
							fprintf(stderr, "error: could not process %s\n", line);
						}
					}
					fclose(fp);
				}
			} else {
				fprintf(stderr, "* bad arg %s\n", argv[i]);
				exit(1);
			}
		} else {
			int itemsAdded = PT_AddIndex(index, argv[i]);
			if (itemsAdded > 0) {
				fprintf(stderr, "processed: %s (%d items added)\n", argv[i], itemsAdded);
			} else if (itemsAdded == 0) {
				fprintf(stderr, "processed: %s (no items added)\n", argv[i]);
			} else {
				fprintf(stderr, "error: could not process %s\n", argv[i]);
			}
		}
	}

	/* sigpipe */
#ifndef _WIN32
  signal( SIGPIPE , sig_brpipe );   // broken pipe (write into non-opened socket)
#endif

  /* Go */
	if (strcmp(argv[1], "--convert") != 0) {
		ret = proxytrack_main(proxyAddr, proxyPort, icpAddr, icpPort, index);
	} else {
		if ((ret = PT_SaveCache(index, argv[2])) == 0) {
			fprintf(stderr, "processed: '%s'\n", argv[2]);
		} else {
			fprintf(stderr, "error: could not save '%s'\n", argv[2]);
		}
	}

	/* Wipe */
	PT_Delete(index);

#ifdef _WIN32
  WSACleanup();
#endif

  return ret;
}

