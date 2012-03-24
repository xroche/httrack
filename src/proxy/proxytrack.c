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


/*

/\/\/\/\/\/\/\/\/\/\/\/\/\ PENDING WORK /\/\/\/\/\/\/\/\/\/\/\/\/\
- Etag update handling
- Other cache archive handling (.arc)
- Live plug/unplug of archives
- Listing
\/\/\/\/\/\/\/\/\/\/\/\/\/ PENDING WORK \/\/\/\/\/\/\/\/\/\/\/\/\/

*/

/*
Architecture rough draft
Xavier Roche 2005

Aim: Building a sub-proxy to be linked with other top level proxies (such as Squid)
Basic design: Classical HTTP/1.0 proxy server, with ICP server support
Internal data design: HTTrack cache indexing in fast hashtables, with 'pluggable' design (add/removal of caches on-the-fly)


Index structure organization:
-----------------------------

foo/hts-cache/new.zip	-----> Index[0]   \
bar/hts-cache/new.zip   -----> Index[1]  >  Central Index Lookup (CIL)
baz/hts-cache/new.zip   -----> Index[2] /
..			-----> ..

Indexes are hashtables with URL (STRING) -> INTEGER lookup.

URL -----> CIL			Ask for index ID
URL -----> Index[ID]		Ask for index properties (ZIP cache index)


Lookup of an entry:
-------------------

ID = CIL[URL]
If ID is valid Then
	return SUCCESS
Else
	return FAILURE
EndIf


Fetching of an entry:
---------------------

RESOURCE = null
ID = CIL[URL]
If ID is valid Then
	OFFSET = Index[ID][URL]
	If OFFSET is valid Then
		RESOURCE = Fetch(ID, OFFSET)
	EndIf
EndIf


Removal of index N:
-------------------

For all entries in Index[N]
	URL(key) -----> Lookup all other caches
		Found: Replace in CIL
		Not Found: Delete entry in CIL
Done
Delete Index[N]


Adding of index N:
------------------

Build Index[N]
For all entries in Index[N]
	URL(key) -----> Lookup in CIL
		Found: Do nothing if corresponding Cache is newer than this one
		Not Found: Add/Replace entry in CIL
Done

Remark: If no cache newer than the added one is found, all entries can be added without any lookup (optim)

*/

/* HTTrack definitions */
#include "htsbase.h"
#include "htsnet.h"
#include "htslib.h"
#include "htsglobal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#ifdef _WIN32
#else
#include <arpa/inet.h>
#endif
/* END specific definitions */

/* String */
#include "proxystrings.h"

/* Network base */
#include "htsbasenet.h"

/* définitions globales */
#include "htsglobal.h"

/* htsweb */
#include "htsinthash.h"

/* ProxyTrack */
#include "proxytrack.h"

/* Store manager */
#include "../minizip/mztools.h"
#include "store.h"

/* threads */
#ifdef _WIN32
#include <process.h>    /* _beginthread, _endthread */
#else
#include <pthread.h>
#endif

/* External references */
// htsErrorCallback htsCallbackErr = NULL;
int htsMemoryFastXfr = 1;    /* fast xfr by default */
void abortLog__fnc(char* msg, char* file, int line);
void abortLog__fnc(char* msg, char* file, int line) {
  FILE* fp = fopen("CRASH.TXT", "wb");
  if (!fp) fp = fopen("/tmp/CRASH.TXT", "wb");
  if (!fp) fp = fopen("C:\\CRASH.TXT", "wb");
  if (!fp) fp = fopen("CRASH.TXT", "wb");
  if (fp) {
    fprintf(fp, "HTTrack " HTTRACK_VERSIONID " closed at '%s', line %d\r\n", file, line);
    fprintf(fp, "Reason:\r\n%s\r\n", msg);
    fflush(fp);
    fclose(fp);
  }
}
// HTSEXT_API t_abortLog abortLog__ = abortLog__fnc;    /* avoid VC++ inlining */
#define webhttrack_lock(A) do{}while(0)

/* Static definitions */

static int linputsoc(T_SOC soc, char* s, int max) {
  int c;
  int j=0;
  do {
    unsigned char ch;
    if (recv(soc, &ch, 1, 0) == 1) {
      c = ch;
    } else {
      c = EOF;
    }
    if (c!=EOF) {
      switch(c) {
        case 13: break;  // sauter CR
        case 10: c=-1; break;
        case 9: case 12: break;  // sauter ces caractères
        default: s[j++]=(char) c; break;
      }
    }
  }  while((c!=-1) && (c!=EOF) && (j<(max-1)));
  s[j]='\0';
  return j;
}

static int check_readinput_t(T_SOC soc, int timeout) {
  if (soc != INVALID_SOCKET) {
    fd_set fds;           // poll structures
    struct timeval tv;          // structure for select
    FD_ZERO(&fds);
    FD_SET(soc,&fds);           
    tv.tv_sec=timeout;
    tv.tv_usec=0;
    select((int)(soc + 1),&fds,NULL,NULL,&tv);
    if (FD_ISSET(soc,&fds))
      return 1;
    else
      return 0;
  } else
    return 0;
}

static int linputsoc_t(T_SOC soc, char* s, int max, int timeout) {
  if (check_readinput_t(soc, timeout)) {
    return linputsoc(soc, s, max);
  }
  return -1;
}

static int gethost(const char* hostname, SOCaddr *server, size_t server_size) {
  if (hostname != NULL && *hostname != '\0') {
#if HTS_INET6==0
		/*
		ipV4 resolver
		*/
		t_hostent* hp=gethostbyname(hostname);
		if (hp!=NULL) {
			if (hp->h_length) {
				SOCaddr_copyaddr(*server, server_size, hp->h_addr_list[0], hp->h_length);
				return 1;
			}
		}
#else
		/*
		ipV6 resolver
		*/
		struct addrinfo* res = NULL;
		struct addrinfo hints;
		memset(&hints, 0, sizeof(hints));
#if 0
		if (IPV6_resolver == 1)        // V4 only (for bogus V6 entries)
			hints.ai_family = PF_INET;
		else if (IPV6_resolver == 2)   // V6 only (for testing V6 only)
			hints.ai_family = PF_INET6;
		else
#endif
		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		if (getaddrinfo(hostname, NULL, &hints, &res) == 0) {
			if (res) {
				if ( (res->ai_addr) && (res->ai_addrlen) ) {
					SOCaddr_copyaddr(*server, server_size, res->ai_addr, res->ai_addrlen);
					freeaddrinfo(res);
					return 1;
				}
			}
		}
		if (res) {
			freeaddrinfo(res);
		}

#endif
	}
	return 0;
}

static String getip(SOCaddr *server, int serverLen) {
	String s = STRING_EMPTY;
#if HTS_INET6==0
	unsigned int sizeMax = sizeof("999.999.999.999:65535");
#else
	unsigned int sizeMax = sizeof("ffff:ffff:ffff:ffff:ffff:ffff:ffff:65535");
#endif
	char * dotted = malloc(sizeMax + 1);
	unsigned short port  = ntohs(SOCaddr_sinport(*server));
	if (dotted == NULL) {
		CRITICAL("memory exhausted");
		return s;
	}
	SOCaddr_inetntoa(dotted, sizeMax, *server, serverLen);
	sprintf(dotted + strlen(dotted), ":%d", port);
	StringAttach(&s, &dotted);
	return s;
}


static T_SOC smallserver_init(const char* adr, int port, int family) {
	SOCaddr server;
	size_t server_size = sizeof(server);

	memset(&server, 0, sizeof(server));
	SOCaddr_initany(server, server_size); 
  if (gethost(adr, &server, server_size)) {    // host name
		T_SOC soc = INVALID_SOCKET;
    if ( (soc = (T_SOC) socket(SOCaddr_sinfamily(server), family, 0)) != INVALID_SOCKET) {
      SOCaddr_initport(server, port);
      if ( bind(soc,(struct sockaddr*) &server, (int)server_size) == 0 ) {
        if (family != SOCK_STREAM 
					|| listen(soc, 10) >=0 ) {
					return soc;
        } else {
#ifdef _WIN32
          closesocket(soc);
#else
          close(soc);
#endif
          soc=INVALID_SOCKET;
        }
      } else {
#ifdef _WIN32
        closesocket(soc);
#else
        close(soc);
#endif
        soc=INVALID_SOCKET;
      }
    }
  }
  return INVALID_SOCKET;
}

static int proxytrack_start(PT_Indexes indexes, T_SOC soc, T_SOC socICP);
int proxytrack_main(char* proxyAddr, int proxyPort, 
										char* icpAddr, int icpPort, 
										PT_Indexes index) {
  int returncode = 0;
  T_SOC soc = smallserver_init(proxyAddr, proxyPort, SOCK_STREAM);
  T_SOC socICP = smallserver_init(proxyAddr, icpPort, SOCK_DGRAM);
  if (soc != INVALID_SOCKET
		&& socICP != INVALID_SOCKET) 
	{
    char url[HTS_URLMAXSIZE*2];
    char method[32];
    char data[32768];
    url[0]=method[0]=data[0]='\0';
    //
    printf("HTTP Proxy installed on %s:%d/\n", proxyAddr, proxyPort);
    printf("ICP Proxy installed on %s:%d/\n", icpAddr, icpPort);
#ifndef _WIN32
    {
      pid_t pid = getpid();
      printf("PID=%d\n", (int)pid);
    }
#endif
    fflush(stdout);
    fflush(stderr);
    //
    if (!proxytrack_start(index, soc, socICP)) {
      int last_errno = errno;
      fprintf(stderr, "Unable to create the server: %s\n", strerror(last_errno));
#ifdef _WIN32
      closesocket(soc);
#else
      close(soc);
#endif
      printf("Done\n");
      returncode = 1;
    } else {
      returncode = 0;
    }
  } else {
    int last_errno = errno;
		fprintf(stderr, "Unable to initialize a temporary server : %s\n", strerror(last_errno));
    returncode = 1;
  }
  printf("EXITED\n");
  fflush(stdout);
  fflush(stderr);
  return returncode;
}

static const char* GetHttpMessage(int statuscode) {
  // Erreurs HTTP, selon RFC
  switch( statuscode) {    
    case 100: return "Continue"; break; 
		case 101: return "Switching Protocols"; break; 
		case 200: return "OK"; break; 
		case 201: return "Created"; break; 
		case 202: return "Accepted"; break; 
		case 203: return "Non-Authoritative Information"; break; 
		case 204: return "No Content"; break; 
		case 205: return "Reset Content"; break; 
		case 206: return "Partial Content"; break; 
		case 207: return "Multi-Status"; break; 
		case 300: return "Multiple Choices"; break; 
		case 301: return "Moved Permanently"; break; 
		case 302: return "Moved Temporarily"; break; 
		case 303: return "See Other"; break; 
		case 304: return "Not Modified"; break; 
		case 305: return "Use Proxy"; break; 
		case 306: return "Undefined 306 error"; break; 
		case 307: return "Temporary Redirect"; break; 
		case 400: return "Bad Request"; break; 
		case 401: return "Unauthorized"; break; 
		case 402: return "Payment Required"; break; 
		case 403: return "Forbidden"; break; 
		case 404: return "Not Found"; break; 
		case 405: return "Method Not Allowed"; break; 
		case 406: return "Not Acceptable"; break; 
		case 407: return "Proxy Authentication Required"; break; 
		case 408: return "Request Time-out"; break; 
		case 409: return "Conflict"; break; 
		case 410: return "Gone"; break; 
		case 411: return "Length Required"; break; 
		case 412: return "Precondition Failed"; break; 
		case 413: return "Request Entity Too Large"; break; 
		case 414: return "Request-URI Too Large"; break; 
		case 415: return "Unsupported Media Type"; break; 
		case 416: return "Requested Range Not Satisfiable"; break; 
		case 417: return "Expectation Failed"; break; 
		case 500: return "Internal Server Error"; break; 
		case 501: return "Not Implemented"; break; 
		case 502: return "Bad Gateway"; break; 
		case 503: return "Service Unavailable"; break; 
		case 504: return "Gateway Time-out"; break; 
		case 505: return "HTTP Version Not Supported"; break; 
		default:	return "Unknown HTTP Error"; break; 
	}
}

#ifndef NO_WEBDAV
static void proxytrack_add_DAV_Item(String *item, String *buff,
																		const char* filename,
																		size_t size,
																		time_t timestamp,
																		const char* mime,
																		int isDir,
																		int isRoot,
																		int isDefault) 
{
	struct tm * timetm;
	if (timestamp == (time_t) 0 || timestamp == (time_t) -1) {
		timestamp = time(NULL);
	}
	if ((timetm = gmtime(&timestamp)) != NULL) {
		char tms[256 + 1];
		const char * name;
		strftime(tms, 256, "%a, %d %b %Y %H:%M:%S GMT", timetm);		/* Sun, 18 Sep 2005 11:45:45 GMT */

		if (mime == NULL || *mime == 0)
			mime = "application/octet-stream";

		StringLength(*buff) = 0;
		escapexml(filename, buff);

		name = strrchr(StringBuff(*buff), '/');
		if (name != NULL)
			name++;
		if (name == NULL || *name == 0) {
			if (strcmp(mime, "text/html") == 0)
				name = "Default Document for the Folder.html";
			else
				name = "Default Document for the Folder";
		}

		StringRoom(*item, 1024);
		sprintf(StringBuffRW(*item),
			"<response xmlns=\"DAV:\">\r\n"
			"<href>/webdav%s%s</href>\r\n"
			"<propstat>\r\n"
			"<prop>\r\n"
			"<displayname>%s</displayname>\r\n"
			"<iscollection>%d</iscollection>\r\n"
			"<haschildren>%d</haschildren>\r\n"
			"<isfolder>%d</isfolder>\r\n"
			"<resourcetype>%s</resourcetype>\r\n"
			"<creationdate>%d-%02d-%02dT%02d:%02d:%02dZ</creationdate>\r\n"
			"<getlastmodified>%s</getlastmodified>\r\n"
			"<supportedlock></supportedlock>\r\n"
			"<lockdiscovery/>\r\n"
			"<getcontenttype>%s</getcontenttype>\r\n"
			"<getcontentlength>%d</getcontentlength>\r\n"
			"<isroot>%d</isroot>\r\n"
			"</prop>\r\n"
			"<status>HTTP/1.1 200 OK</status>\r\n"
			"</propstat>\r\n"
			"</response>\r\n",
			/* */
			( StringBuff(*buff)[0] == '/' ) ? "" : "/", StringBuff(*buff),
			name,
			isDir ? 1 : 0,
			isDir ? 1 : 0,
			isDir ? 1 : 0,
			isDir ? "<collection/>" : "",
			timetm->tm_year + 1900, timetm->tm_mon + 1, timetm->tm_mday, timetm->tm_hour, timetm->tm_min, timetm->tm_sec,
			tms,
			isDir ? "httpd/unix-directory" : mime,
			(int)size,
			isRoot ? 1 : 0
			);
		StringLength(*item) = (int) strlen(StringBuff(*item));
	}
}

/* Convert a RFC822 time to time_t */
time_t get_time_rfc822(const char* s) {
  struct tm result;
  /* */
  char months[]="jan feb mar apr may jun jul aug sep oct nov dec";
  char str[256];
  char* a;
	int i;
  /* */
  int result_mm=-1;
  int result_dd=-1;
  int result_n1=-1;
  int result_n2=-1;
  int result_n3=-1;
  int result_n4=-1;
  /* */

  if ((int) strlen(s) > 200)
    return (time_t)0;
	for(i = 0 ; s[i] != 0 ; i++) {
		if (s[i] >= 'A' && s[i] <= 'Z') 
			str[i] = s[i] + ('a' - 'A');
		else
			str[i] = s[i];
	}
	str[i] = 0;
  /* éliminer :,- */
  while( (a=strchr(str,'-')) ) *a=' ';
  while( (a=strchr(str,':')) ) *a=' ';
  while( (a=strchr(str,',')) ) *a=' ';
  /* tokeniser */
  a=str;
  while(*a) {
    char *first,*last;
    char tok[256];
    /* découper mot */
    while(*a==' ') a++;   /* sauter espaces */
    first=a;
    while((*a) && (*a!=' ')) a++;
    last=a;
    tok[0]='\0';
    if (first!=last) {
      char* pos;
      strncat(tok,first,(int) (last - first));
      /* analyser */
      if ( (pos=strstr(months,tok)) ) {               /* month always in letters */
        result_mm=((int) (pos - months))/4;
      } else {
        int number;
        if (sscanf(tok,"%d",&number) == 1) {      /* number token */
          if (result_dd<0)                        /* day always first number */
            result_dd=number;
          else if (result_n1<0)
            result_n1=number;
          else if (result_n2<0)
            result_n2=number;
          else if (result_n3<0)
            result_n3=number;
          else if (result_n4<0)
            result_n4=number;
        }   /* sinon, bruit de fond(+1GMT for exampel) */
      }
    }
  }
  if ((result_n1>=0) && (result_mm>=0) && (result_dd>=0) && (result_n2>=0) && (result_n3>=0) && (result_n4>=0)) {
    if (result_n4>=1000) {               /* Sun Nov  6 08:49:37 1994 */
      result.tm_year=result_n4-1900;
      result.tm_hour=result_n1;
      result.tm_min=result_n2;
      result.tm_sec=max(result_n3,0);
    } else {                            /* Sun, 06 Nov 1994 08:49:37 GMT or Sunday, 06-Nov-94 08:49:37 GMT */
      result.tm_hour=result_n2;
      result.tm_min=result_n3;
      result.tm_sec=max(result_n4,0);
      if (result_n1<=50)                /* 00 means 2000 */
        result.tm_year=result_n1+100;
      else if (result_n1<1000)          /* 99 means 1999 */
        result.tm_year=result_n1;
      else                              /* 2000 */
        result.tm_year=result_n1-1900;
    }
    result.tm_isdst=0;        /* assume GMT */
    result.tm_yday=-1;        /* don't know */
    result.tm_wday=-1;        /* don't know */
    result.tm_mon=result_mm;
    result.tm_mday=result_dd;
    return mktime(&result);
  }
  return (time_t) 0;
}

static PT_Element proxytrack_process_DAV_Request(PT_Indexes indexes, const char * urlFull, int depth) {
	const char * file = jump_protocol_and_auth(urlFull);
	if ( (file = strchr(file, '/')) == NULL)
		return NULL;

	if (strncmp(file, "/webdav", 7) != 0) {
		PT_Element elt = PT_ElementNew();
		elt->statuscode = 405;
		strcpy(elt->msg, "Method Not Allowed");
		return elt;
	}

	/* Skip /webdav */
	file += 7;

	/* */
	{
		PT_Element elt = PT_ElementNew();
		int i, isDir;
		String url = STRING_EMPTY;
		String response = STRING_EMPTY;
		String item = STRING_EMPTY;
		String itemUrl = STRING_EMPTY;
		String buff = STRING_EMPTY;
		StringClear(response);
		StringClear(item);
		StringClear(itemUrl);
		StringClear(buff);

		/* Canonize URL */
		StringCopy(url, file + ((file[0] == '/') ? 1 : 0));
		if (StringLength(url) > 0) {
			if (StringBuff(url)[StringLength(url) - 1] == '/') {
				StringBuffRW(url)[StringLength(url) - 1] = '\0';
				StringLength(url)--;
			}
		}

		/* Form response */
		StringRoom(response, 1024);
		sprintf(StringBuffRW(response),
			"<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
			"<multistatus xmlns=\"DAV:\">\r\n");
		StringLength(response) = (int) strlen(StringBuff(response));
		/* */

		/* Root */
		StringLength(item) = 0;
		proxytrack_add_DAV_Item(&item, &buff, 
			StringBuff(url), /*size*/0, /*timestamp*/(time_t) 0, /*mime*/NULL, /*isDir*/1, /*isRoot*/1, /*isDefault*/0);
		StringMemcat(response, StringBuff(item), StringLength(item));

		/* Childrens (Depth > 0) */
		if (depth > 0) {
			time_t timestampRep = (time_t) -1;
			const char * prefix = StringBuff(url);
			unsigned int prefixLen = (unsigned int) strlen(prefix);
			char ** list = PT_Enumerate(indexes, prefix, 0);
			if (list != NULL) {
				for(isDir = 1 ; isDir >= 0 ; isDir--) {
					for(i = 0 ; list[i] != NULL ; i++) {
						const char * thisUrl = list[i];
						const char * mimeType = "application/octet-stream";
						unsigned int thisUrlLen = (unsigned int) strlen(thisUrl);
						int thisIsDir = (thisUrl[thisUrlLen - 1] == '/') ? 1 : 0;

						/* Item URL */
						StringRoom(itemUrl, thisUrlLen + prefixLen + sizeof("/webdav/") + 1);
						StringClear(itemUrl);
						sprintf(StringBuffRW(itemUrl), "/%s/%s", prefix, thisUrl);
						if (!thisIsDir)
							StringLength(itemUrl) = (int) strlen(StringBuff(itemUrl));
						else
							StringLength(itemUrl) = (int) strlen(StringBuff(itemUrl)) - 1;
						StringBuffRW(itemUrl)[StringLength(itemUrl)] = '\0';

						if (thisIsDir == isDir) {
							size_t size = 0;
							time_t timestamp = (time_t) 0;
							PT_Element file = NULL;

							/* Item stats */
							if (!isDir) {
								file = PT_ReadIndex(indexes, StringBuff(itemUrl) + 1, FETCH_HEADERS);
								if (file != NULL && file->statuscode == HTTP_OK ) {
									size = file->size;
									if (file->lastmodified) {
										timestamp = get_time_rfc822(file->lastmodified);
									}
									if (timestamp == (time_t) 0) {
										if (timestampRep == (time_t) -1) {
											timestampRep = 0;
											if (file->indexId != -1) {
												timestampRep = PT_Index_Timestamp(PT_GetIndex(indexes, file->indexId));
											}
										}
										timestamp = timestampRep;
									}
									if (file->contenttype) {
										mimeType = file->contenttype;
									}
								}
							}

							/* Add item */
							StringLength(item) = 0;
							proxytrack_add_DAV_Item(&item, &buff, 
								StringBuff(itemUrl), size, timestamp, mimeType, isDir, /*isRoot*/0, /*isDefault*/(thisUrlLen == 0));
							StringMemcat(response, StringBuff(item), StringLength(item));

							/* Wipe element */
							if (file != NULL)
								PT_Element_Delete(&file);
						}
					}
				}
				PT_Enumerate_Delete(&list);
			}  /* items != NULL */
		}		/* Depth > 0 */

		/* End of responses */
		StringCat(response,
			"</multistatus>\r\n"
			);

		StringFree(item);
		StringFree(itemUrl);
		StringFree(url);
		StringFree(buff);

		elt->size = StringLength(response);
		elt->adr = StringAcquire(&response);
		elt->statuscode = 207;									/* Multi-Status */
		strcpy(elt->charset, "utf-8");
		strcpy(elt->contenttype, "text/xml");
		strcpy(elt->msg, "Multi-Status");
		StringFree(response);

		fprintf(stderr, "RESPONSE:\n%s\n", elt->adr);

		return elt;
	}
	return NULL;
}
#endif

static PT_Element proxytrack_process_HTTP_List(PT_Indexes indexes, const char * url) {
	char ** list = PT_Enumerate(indexes, url, 0);
	if (list != NULL) {
		PT_Element elt = PT_ElementNew();
		int i, isDir;
		String html = STRING_EMPTY;
		StringClear(html);
		StringCat(html, 
			"<html>"
			PROXYTRACK_COMMENT_HEADER
			DISABLE_IE_FRIENDLY_HTTP_ERROR_MESSAGES
			"<head>\r\n"
			"<title>ProxyTrack " PROXYTRACK_VERSION " Catalog</title>"
			"</head>\r\n"
			"<body>\r\n"
			"<h3>Directory index:</h3><br />"
			"<br />"
			"<hr>"
			"<tt>[DIR] <a href=\"..\">..</a></tt><br />"
			);
		for(isDir = 1 ; isDir >= 0 ; isDir--) {
			for(i = 0 ; list[i] != NULL ; i++) {
				char * thisUrl = list[i];
				unsigned int thisUrlLen = (unsigned int) strlen(thisUrl);
				int thisIsDir = (thisUrl[thisUrlLen - 1] == '/') ? 1 : 0;
				if (thisIsDir == isDir) {
					if (isDir)
						StringCat(html, "<tt>[DIR] ");
					else
						StringCat(html, "<tt>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;");
					StringCat(html, "<a href=\"");
					if (isDir) {
						StringCat(html, "http://proxytrack/");
					}
					StringCat(html, url);
					StringCat(html, list[i]);
					StringCat(html, "\">");
					StringCat(html, list[i]);
					StringCat(html, "</a></tt><br />");
				}
			}
		}
		StringCat(html, 
			"</body>"
			"</html>");
		PT_Enumerate_Delete(&list);
		elt->size = StringLength(html);
		elt->adr = StringAcquire(&html);
		elt->statuscode = HTTP_OK;
		strcpy(elt->charset, "iso-8859-1");
		strcpy(elt->contenttype, "text/html");
		strcpy(elt->msg, "OK");
		StringFree(html);
		return elt;
	}
	return NULL;
}

static void proxytrack_process_HTTP(PT_Indexes indexes, T_SOC soc_c) {
  int timeout=30;
  int retour=0;
	int willexit=0;
	int buffer_size = 32768;
	char * buffer = (char*)malloc(buffer_size);
	int line1Size = 1024;
	char * line1 = (char*)malloc(line1Size);
	int lineSize = 8192;
	char * line = (char*)malloc(lineSize);
	int length = 0;
	int keepAlive = 1;

	String url = STRING_EMPTY;
	String urlRedirect = STRING_EMPTY;
	String headers = STRING_EMPTY;
	String output = STRING_EMPTY;
	String host = STRING_EMPTY;
	String localhost = STRING_EMPTY;
#ifndef NO_WEBDAV
	String davHeaders = STRING_EMPTY;
	String davRequest = STRING_EMPTY;
#endif

	StringRoom(localhost, 256);
	if (gethostname(StringBuffRW(localhost), (int) StringCapacity(localhost) - 1) == 0) {
		StringLength(localhost) = (int) strlen(StringBuff(localhost));
	} else {
		StringCopy(localhost, "localhost");
	}

#ifdef _DEBUG
	Sleep(1000);
#endif

	if (buffer == NULL || line == NULL || line1 == NULL) {
		CRITICAL("proxytrack_process_HTTP:memory exhausted");
#ifdef _WIN32
		closesocket(soc_c);
#else
		close(soc_c);
#endif
		return ;
	}

	do {
		const char* msgError = NULL;
		int msgCode = 0;
		PT_Element element = NULL;
		char* command;
		char* proto;
		char* surl;
		int directHit = 0;
		int headRequest = 0;
		int listRequest = 0;
#ifndef NO_WEBDAV
		int davDepth = 0;
#endif

		/* Clear context */
		line[0] = line1[0] = '\0';
		buffer[0] = '\0';
		command = line1;
		StringClear(url);
		StringClear(urlRedirect);
		StringClear(headers);
		StringClear(output);
		StringClear(host);
#ifndef NO_WEBDAV
		StringClear(davHeaders);
		StringClear(davRequest);
#endif

		/* line1: "GET http://www.example.com/ HTTP/1.0" */
		if (linputsoc_t(soc_c, line1, line1Size - 2, timeout) > 0
			&& ( surl = strchr(line1, ' ') )
			&& !(*surl = '\0') 
			&& ++surl 
			&& (proto = strchr(surl, ' ')) && !(*proto = '\0') && ++proto) 
		{
			/* Flush headers */
			while(linputsoc_t(soc_c, line, lineSize - 2, timeout) > 0
				&& line[0] != 0) 
			{
				int p;
				if ((p = strfield(line, "Content-length:"))!=0) {
					if (sscanf(line+p, "%d", &length) != 1) {
						msgCode = 500;
						msgError = "Bad HTTP Content-Length Field";
						keepAlive = 0;
						length = 0;
					}
				} else if (strcasecmp(line, "Connection: close") == 0) {
					keepAlive = 0;
				} else if (strcasecmp(line, "Connection: keep-alive") == 0) {
					keepAlive = 1;
				} else if ((p = strfield(line, "Host:"))) {
					char* chost = line + p;
					if (*chost == ' ')
						chost++;
					StringCopy(host, chost);
				}
#ifndef NO_WEBDAV
				else if ((p = strfield(line, "Depth: "))) {
					char* depth = line + p;
					if (sscanf(depth, "%d", &davDepth) != 1) {
						davDepth = 0;
					}
				}
#endif
			}

			/* Flush body */
#ifndef NO_WEBDAV
			if (length > 0) {
				if (length < 32768) {
					StringRoom(davRequest, length + 1);
					if (recv(soc_c, StringBuffRW(davRequest), length, 0) == length) {
						StringBuffRW(davRequest)[length] = 0;
					} else {
						msgCode = 500;
						msgError = "Posted Data Read Error";
						keepAlive = 0;
					}
				} else {
					msgCode = 500;
					msgError = "Posted Data Too Large";
					keepAlive = 0;
				}
			}
#endif

			/* Switch protocol ID */
			if (strcasecmp(command, "post") == 0) {
#ifndef NO_WEBDAV
				msgCode = 404;
#else
				msgCode = 501;
				keepAlive = 0;
#endif
				msgError = "Proxy Error (POST Request Forbidden)";
			}
			else if (strcasecmp(command, "get") == 0) {
				headRequest = 0;
			}
			else if (strcasecmp(command, "head") == 0) {
				headRequest = 1;
			}
#ifndef NO_WEBDAV
			else if (strcasecmp(command, "options") == 0) {
				const char * options = "GET, HEAD, OPTIONS, POST, PROPFIND, TRACE"
					", MKCOL, DELETE, PUT";			/* Not supported */
				msgCode = HTTP_OK;
				StringRoom(headers, 8192);
				sprintf(StringBuffRW(headers), 
					"HTTP/1.1 %d %s\r\n"
					"DAV: 1, 2\r\n"
					"MS-Author-Via: DAV\r\n"
					"Cache-Control: private\r\n"
					"Allow: %s\r\n",
					msgCode, GetHttpMessage(msgCode), options);
				StringLength(headers) = (int) strlen(StringBuff(headers));
			}
			else if (strcasecmp(command, "propfind") == 0) {
				if (davDepth > 1) {
					msgCode = 403;
					msgError = "DAV Depth Limit Forbidden";
				} else {
					fprintf(stderr, "DEBUG: DAV-DATA=<%s>\n", StringBuff(davRequest));
					listRequest = 2;				/* propfind */
				}
			}
			else if (strcasecmp(command, "mkcol") == 0
				|| strcasecmp(command, "delete") == 0
				|| strcasecmp(command, "put") == 0
				|| strcasecmp(command, "proppatch") == 0
				|| strcasecmp(command, "lock") == 0
				|| strcasecmp(command, "unlock") == 0
				|| strcasecmp(command, "copy") == 0
				|| strcasecmp(command, "trace") == 0)
			{
				msgCode = 403;
				msgError = "Method Forbidden";
			}
#endif
			else {
				msgCode = 501;
				msgError = "Proxy Error (Unsupported or Unknown HTTP Command Request)";
				keepAlive = 0;
			}
			if (strcasecmp(proto, "http/1.1") == 0) {
				keepAlive = 1;
			} else if (strcasecmp(proto, "http/1.0") == 0) {
				keepAlive = 0;
			} else {
				msgCode = 505;
				msgError = "Proxy Error (Unknown HTTP Version)";
				keepAlive = 0;
			}

			/* Post-process request */
			if (link_has_authority(surl)) {
				if (strncasecmp(surl, "http://proxytrack/", sizeof("http://proxytrack/") - 1) == 0) {
					directHit = 1; /* Another direct hit hack */
				}
				StringCopy(url, surl);
			} else {
				if (StringLength(host) > 0) {
					/* Direct hit */
					if (
#ifndef NO_WEBDAV
						listRequest != 2
						&&
#endif
						strncasecmp(StringBuff(host), StringBuff(localhost), StringLength(localhost)) == 0
						&& 
						(StringBuff(host)[StringLength(localhost)] == '\0'
						|| StringBuff(host)[StringLength(localhost)] == ':')
						&& surl[0] == '/'
						)
					{
						const char * toHit = surl + 1;
						if (strncmp(toHit, "webdav/", 7) == 0) {
							toHit += 7;
						}
						/* Direct hit */
						directHit = 1;
						StringCopy(url, "");
						if (!link_has_authority(toHit))
							StringCat(url, "http://");
						StringCat(url, toHit);
					} else if (strncasecmp(surl, "/proxytrack/", sizeof("/proxytrack/") - 1) == 0) {
						const char * toHit = surl + sizeof("/proxytrack/") - 1;
						/* Direct hit */
						directHit = 1;
						StringCopy(url, "");
						if (!link_has_authority(toHit))
							StringCat(url, "http://");
						StringCat(url, toHit);
					} else {
						/* Transparent proxy */
						StringCopy(url, "http://");
						StringCat(url, StringBuff(host));
						StringCat(url, surl);
					}
				} else {
					msgCode = 500;
					msgError = "Transparent Proxy Error ('Host' HTTP Request Header Field Missing)";
					keepAlive = 0;
				}
			}

			/* Response */
			if (msgCode == 0) {
				if (listRequest == 1) {
					element = proxytrack_process_HTTP_List(indexes, StringBuff(url));
				}
#ifndef NO_WEBDAV
				else if (listRequest == 2) {
					if ((element = proxytrack_process_DAV_Request(indexes, StringBuff(url), davDepth)) != NULL) {
						msgCode = element->statuscode;
						StringRoom(davHeaders, 1024);
						sprintf(StringBuffRW(davHeaders), 
							"DAV: 1, 2\r\n"
							"MS-Author-Via: DAV\r\n"
							"Cache-Control: private\r\n");
						StringLength(davHeaders) = (int) strlen(StringBuff(davHeaders));
					}
				}
#endif
				else {
					element = PT_ReadIndex(indexes, StringBuff(url), FETCH_BODY);
				}
				if (element == NULL
#ifndef NO_WEBDAV
					&& listRequest == 2
#endif
					&& StringLength(url) > 0 
					&& StringBuff(url)[StringLength(url) - 1] == '/'
					)
				{
					element = PT_Index_HTML_BuildRootInfo(indexes);
					if (element != NULL) {
						element->statuscode = 404;				/* HTML page, but in error */
					}
				}
				if (element != NULL) {
					msgCode = element->statuscode;
					StringRoom(headers, 8192);
					sprintf(StringBuffRW(headers), 
						"HTTP/1.1 %d %s\r\n"
#ifndef NO_WEBDAV
						"%s"
#endif
						"Content-Type: %s%s%s%s\r\n"
						"%s%s%s"
						"%s%s%s"
						"%s%s%s",
						/* */
						msgCode,
						element->msg,
#ifndef NO_WEBDAV
						/* DAV */
						StringBuff(davHeaders),
#endif
						/* Content-type: foo; [ charset=bar ] */
						element->contenttype,
						( ( element->charset[0]) ? "; charset=\"" : ""),
						element->charset,
						( ( element->charset[0]) ? "\"" : ""),
						/* location */
						( ( element->location != NULL && element->location[0]) ? "Location: " : ""),
						( ( element->location != NULL && element->location[0]) ? element->location : ""),
						( ( element->location != NULL && element->location[0]) ? "\r\n" : ""),
						/* last-modified */
						( ( element->lastmodified[0]) ? "Last-Modified: " : ""),
						( ( element->lastmodified[0]) ? element->lastmodified : ""),
						( ( element->lastmodified[0]) ? "\r\n" : ""),
						/* etag */
						( ( element->etag[0]) ? "ETag: " : ""),
						( ( element->etag[0]) ? element->etag : ""),
						( ( element->etag[0]) ? "\r\n" : "")
						);
					StringLength(headers) = (int) strlen(StringBuff(headers));
				} else {
					/* No query string, no ending / : check the the <url>/ page */
					if (StringLength(url) > 0 && StringBuff(url)[StringLength(url) - 1] != '/' && strchr(StringBuff(url), '?') == NULL) {
						StringCopy(urlRedirect, StringBuff(url));
						StringCat(urlRedirect, "/");
						if (PT_LookupIndex(indexes, StringBuff(urlRedirect))) {
							msgCode = 301;  /* Moved Permanently */
							StringRoom(headers, 8192);
							sprintf(StringBuffRW(headers),
								"HTTP/1.1 %d %s\r\n"
								"Content-Type: text/html\r\n"
								"Location: %s\r\n",
								/* */
								msgCode,
								GetHttpMessage(msgCode),
								StringBuff(urlRedirect)
								);
							StringLength(headers) = (int) strlen(StringBuff(headers));
							/* */
							StringRoom(output, 1024 + sizeof(PROXYTRACK_COMMENT_HEADER) + sizeof(DISABLE_IE_FRIENDLY_HTTP_ERROR_MESSAGES));
							sprintf(StringBuffRW(output), 
								"<html>"
								PROXYTRACK_COMMENT_HEADER
								DISABLE_IE_FRIENDLY_HTTP_ERROR_MESSAGES
								"<head>"
								"<title>ProxyTrack - Page has moved</title>"
								"</head>\r\n"
								"<body>"
								"<h3>The correct location is:</h3><br />"
								"<b><a href=\"%s\">%s</a></b><br />"
								"<br />"
								"<br />\r\n"
								"<i>Generated by ProxyTrack " PROXYTRACK_VERSION ", (C) Xavier Roche and other contributors</i>"
								"\r\n"
								"</body>"
								"</header>",
								StringBuff(urlRedirect),
								StringBuff(urlRedirect));
							StringLength(output) = (int) strlen(StringBuff(output));
						}
					}
					if (msgCode == 0) {
						msgCode = 404;
						msgError = "Not Found in this cache";
					}
				}
			}
		} else {
			msgCode = 500;
			msgError = "Server Error";
			keepAlive = 0;
		}
		if (StringLength(headers) == 0) {
			if (msgCode == 0) {
				msgCode = 500;
				msgError = "Internal Proxy Error";
			} else if (msgError == NULL) {
				msgError = GetHttpMessage(msgCode);
			}
			StringRoom(headers, 256);
			sprintf(StringBuffRW(headers), 
				"HTTP/1.1 %d %s\r\n"
				"Content-type: text/html\r\n",
				msgCode,
				msgError);
			StringLength(headers) = (int) strlen(StringBuff(headers));
			StringRoom(output, 1024 + sizeof(PROXYTRACK_COMMENT_HEADER) + sizeof(DISABLE_IE_FRIENDLY_HTTP_ERROR_MESSAGES));
			sprintf(StringBuffRW(output), 
				"<html>"
				PROXYTRACK_COMMENT_HEADER
				DISABLE_IE_FRIENDLY_HTTP_ERROR_MESSAGES
				"<head>"
				"<title>ProxyTrack - HTTP Proxy Error %d</title>"
				"</head>\r\n"
				"<body>"
				"<h3>A proxy error has occured while processing the request.</h3><br />"
				"<b>Error HTTP %d: <i>%s</i></b><br />"
				"<br />"
				"<br />\r\n"
				"<i>Generated by ProxyTrack " PROXYTRACK_VERSION ", (C) Xavier Roche and other contributors</i>"
				"\r\n"
				"</body>"
				"</html>",
				msgCode,
				msgCode,
				msgError);
			StringLength(output) = (int) strlen(StringBuff(output));
		}
		{
			char tmp[20 + 1]; /* 2^64 = 18446744073709551616 */
			size_t dataSize = 0;
			if (!headRequest) {
				dataSize = StringLength(output);
				if (dataSize == 0 && element != NULL) {
					dataSize = element->size;
				}
			}
			sprintf(tmp, "%d", (int) dataSize);
			StringCat(headers, "Content-length: ");
			StringCat(headers, tmp);
			StringCat(headers, "\r\n");
		}
		if (keepAlive) {
			StringCat(headers, 
				"Connection: Keep-Alive\r\n"
				"Proxy-Connection: Keep-Alive\r\n");
		} else {
			StringCat(headers, 
				"Connection: Close\r\n"
				"Proxy-Connection: Close\r\n");
		}
		if (msgCode != 500)
			StringCat(headers, "X-Cache: HIT from ");
		else
			StringCat(headers, "X-Cache: MISS from ");
		StringCat(headers, StringBuff(localhost));
		StringCat(headers, "\r\n");

		/* Logging */
		{
			const char * contentType = "text/html";
			size_t size = StringLength(output) ? StringLength(output) : ( element ? element->size : 0 );
			/* */
			String ip = STRING_EMPTY;
			SOCaddr serverClient;
			int lenServerClient = (int) sizeof(serverClient);
			memset(&serverClient, 0, sizeof(serverClient));
			if (getsockname(soc_c, (struct sockaddr*) &serverClient, &lenServerClient) == 0) {
				ip = getip(&serverClient, lenServerClient);
			} else {
				StringCopy(ip, "unknown");
			}
			if (element != NULL && element->contenttype[0] != '\0') {
				contentType = element->contenttype;
			}
			LOG("HTTP %s %d %d %s %s %s" _ StringBuff(ip) _ msgCode _ (int)size _ command _ StringBuff(url) _ contentType);
			StringFree(ip);
		}

		/* Send reply */
		StringCat(headers, "Server: ProxyTrack " PROXYTRACK_VERSION " (HTTrack " HTTRACK_VERSIONID ")\r\n");
		StringCat(headers, "\r\n");			/* Headers separator */
		if (send(soc_c, StringBuff(headers), (int)StringLength(headers), 0) != StringLength(headers)
			|| ( !headRequest && StringLength(output) > 0 && send(soc_c, StringBuff(output), (int)StringLength(output), 0) != StringLength(output))
			|| ( !headRequest && StringLength(output) == 0 && element != NULL && element->adr != NULL && send(soc_c, element->adr, (int)element->size, 0) != element->size)
			)
		{
			keepAlive = 0;		/* Error, abort connection */
		}
		PT_Element_Delete(&element);

		/* Shutdown (FIN) and wait until confirmed */
		if (!keepAlive) {
			char c;
#ifdef _WIN32
			shutdown(soc_c, SD_SEND);
#else
			shutdown(soc_c, 1);
#endif
			while(recv(soc_c, ((char*)&c), 1, 0) > 0);
		}
	} while(keepAlive);

#ifdef _WIN32
	closesocket(soc_c);
#else
	close(soc_c);
#endif

	StringFree(url);
	StringFree(urlRedirect);
	StringFree(headers);
	StringFree(output);
	StringFree(host);

	if (buffer)
		free(buffer);
}

/* Generic threaded function start */
static int startThread(void (*funct)(void*), void* param) 
{
	if (param != NULL) {
#ifdef _WIN32
		if (_beginthread(funct, 0, param) == -1) {
			free(param);
			return 0;
		}
		return 1;
#else
		pthread_t handle = 0;
		int retcode;
		retcode = pthread_create(&handle, NULL, funct, param);
		if (retcode != 0) {   /* error */
			free(param);
			return 0;
		} else {
			/* detach the thread from the main process so that is can be independent */
			pthread_detach(handle);
			return 1;
		}
#endif
	} else {
		return 0;
	}
}

/* Generic socket/index structure */
typedef struct proxytrack_process_th_p {
	T_SOC soc_c;
	PT_Indexes indexes;
	void (*process)(PT_Indexes indexes, T_SOC soc_c);
} proxytrack_process_th_p;

/* Generic socket/index function stub */
static void proxytrack_process_th(void* param_) {
	proxytrack_process_th_p *param = (proxytrack_process_th_p *) param_;
	T_SOC soc_c = param->soc_c;
	PT_Indexes indexes = param->indexes;
	void (*process)(PT_Indexes indexes, T_SOC soc_c) = param->process;
	free(param);
	process(indexes, soc_c);
	return ;
}

/* Process generic socket/index operation */
static int proxytrack_process_generic(void (*process)(PT_Indexes indexes, T_SOC soc_c),
																			PT_Indexes indexes, T_SOC soc_c) 
{
	proxytrack_process_th_p *param = calloc(sizeof(proxytrack_process_th_p), 1);
	if (param != NULL) {
		param->soc_c = soc_c;
		param->indexes = indexes;
		param->process = process;
		return startThread(proxytrack_process_th, param);
	} else {
		CRITICAL("proxytrack_process_generic:Memory exhausted");
		return 0;
	}
	return 0;
}

/* Process HTTP proxy requests */
static int proxytrack_process_HTTP_threaded(PT_Indexes indexes, T_SOC soc) {
	return proxytrack_process_generic(proxytrack_process_HTTP, indexes, soc);
}

/* HTTP Server */
static int proxytrack_start_HTTP(PT_Indexes indexes, T_SOC soc) {
	while(soc != INVALID_SOCKET) {
    T_SOC soc_c;
		if ( (soc_c = (T_SOC) accept(soc, NULL, NULL)) != INVALID_SOCKET) {
			if (!proxytrack_process_HTTP_threaded(indexes, soc_c)) {
				CRITICAL("proxytrack_start_HTTP::Can not fork a thread");
			}
		}
	}
	if (soc != INVALID_SOCKET) {
#ifdef _WIN32
		closesocket(soc);
#else
		close(soc);
#endif
	}
	return 1;
}

/* Network order is big endian */
#define READ_NET16(buffer) ( ( ((unsigned char*)buffer)[0] << 8 ) + ((unsigned char*)buffer)[1] )
#define READ_NET32(buffer) ( ( READ_NET16(buffer) << 16 ) + READ_NET16(((unsigned char*)buffer) + 2) )
#define WRITE_NET8(buffer, value) do { \
	((unsigned char*)buffer)[0] = (unsigned char)(value); \
} while(0)
#define WRITE_NET16(buffer, value) do { \
	((unsigned char*)buffer)[0] = (((unsigned short)(value)) >> 8) & 0xff; \
	((unsigned char*)buffer)[1] = ((unsigned short)(value)) & 0xff; \
} while(0)
#define WRITE_NET32(buffer, value) do { \
	WRITE_NET16(buffer, ( ((unsigned int)(value)) >> 16 ) & 0xffff); \
	WRITE_NET16(((unsigned char*)buffer) + 2, ( ((unsigned int)(value)) ) & 0xffff); \
} while(0)

static int ICP_reply(struct sockaddr * clientAddr,
										 int clientAddrLen,
										 T_SOC soc,
										 /* */
										 unsigned char  Opcode,
										 unsigned char  Version,
										 unsigned short Message_Length,
										 unsigned int   Request_Number,
										 unsigned int   Options,
										 unsigned int   Option_Data,
										 unsigned int   Sender_Host_Address,
										 unsigned char *Message
										 ) 
{
	int ret = 0;
	unsigned long int BufferSize;
	unsigned char * buffer;
	if (Message_Length == 0 && Message != NULL)								/* We have to get the message size */
		Message_Length  = (unsigned int) strlen(Message) + 1;		/* NULL terminated */
	BufferSize = 20 + Message_Length;
	buffer = malloc(BufferSize);
	if (buffer != NULL) {
		WRITE_NET8(&buffer[0], Opcode);
		WRITE_NET8(&buffer[1], Version);
		WRITE_NET16(&buffer[2], Message_Length);
		WRITE_NET32(&buffer[4], Request_Number);
		WRITE_NET32(&buffer[8], Options);
		WRITE_NET32(&buffer[12], Option_Data);
		WRITE_NET32(&buffer[16], Sender_Host_Address);
		if (Message != NULL && Message_Length > 0) {
			memcpy(buffer + 20, Message, Message_Length);
		}
		if (sendto(soc, buffer, BufferSize, 0, clientAddr, clientAddrLen) == BufferSize) {
			ret = 1;
		}
		free(buffer);
	}
	return ret;
}

/* ICP Server */
static int proxytrack_start_ICP(PT_Indexes indexes, T_SOC soc) {
	/* "ICP messages MUST not exceed 16,384 octets in length." (RFC2186) */
	int bufferSize = 16384;
	unsigned char * buffer = (unsigned char*) malloc(bufferSize + 1);
	if (buffer == NULL) {
		CRITICAL("proxytrack_start_ICP:memory exhausted");
#ifdef _WIN32
		closesocket(soc);
#else
		close(soc);
#endif
		return -1;
	}
	while(soc != INVALID_SOCKET) {
		struct sockaddr clientAddr;
		int clientAddrLen = sizeof(struct sockaddr);
		int n;
		memset(&clientAddr, 0, sizeof(clientAddr));
		n = recvfrom(soc, (char*)buffer, bufferSize, 0, &clientAddr, &clientAddrLen);
		if (n != -1) {
			const char * LogRequest = "ERROR";
			const char * LogReply = "ERROR";
			unsigned char * UrlRequest = NULL;
			if (n >= 20) {
				enum {
					ICP_OP_MIN = 0,
					ICP_OP_INVALID = 0,
					ICP_OP_QUERY = 1,
					ICP_OP_HIT = 2,
					ICP_OP_MISS = 3,
					ICP_OP_ERR = 4,
					ICP_OP_SECHO = 10,
					ICP_OP_DECHO = 11,
					ICP_OP_MISS_NOFETCH = 21,
					ICP_OP_DENIED = 22,
					ICP_OP_HIT_OBJ = 23,
					ICP_OP_MAX = ICP_OP_HIT_OBJ
				};
				unsigned char  Opcode = buffer[0];
				unsigned char  Version = buffer[1];
				unsigned short Message_Length = READ_NET16(&buffer[2]);
				unsigned int   Request_Number = READ_NET32(&buffer[4]);   /* Session ID */
				unsigned int   Options = READ_NET32(&buffer[8]);
				unsigned int   Option_Data = READ_NET32(&buffer[12]);     /* ICP_FLAG_SRC_RTT */
				unsigned int   Sender_Host_Address = READ_NET32(&buffer[16]);		/* ignored */
				unsigned char* Payload = &buffer[20];
				buffer[bufferSize] = '\0';															  /* Ensure payload is NULL terminated */
				if (Message_Length <= bufferSize - 20) {
					if (Opcode >= ICP_OP_MIN && Opcode <= ICP_OP_MAX) {
						if (Version == 2) {
							switch(Opcode) {
							case ICP_OP_QUERY:
								{
									unsigned int UrlRequestSize;
									UrlRequest = &Payload[4];
									UrlRequestSize = (unsigned int)strlen((char*)UrlRequest);
									LogRequest = "ICP_OP_QUERY";
									if (indexes == NULL) {
										ICP_reply(&clientAddr, clientAddrLen, soc, ICP_OP_DENIED, Version, 0, Request_Number, 0, 0, 0, UrlRequest);
										LogReply = "ICP_OP_DENIED";
									} else if (PT_LookupIndex(indexes, UrlRequest)) {
										ICP_reply(&clientAddr, clientAddrLen, soc, ICP_OP_HIT, Version, 0, Request_Number, 0, 0, 0, UrlRequest);
										LogReply = "ICP_OP_HIT";
									} else {
										if (UrlRequestSize > 0 && UrlRequest[UrlRequestSize - 1] != '/' && strchr(UrlRequest, '?') == NULL) {
											char * UrlRedirect = malloc(UrlRequestSize + 1 + 1);
											if (UrlRedirect != NULL) {
												sprintf(UrlRedirect, "%s/", UrlRequest);
												if (PT_LookupIndex(indexes, UrlRedirect)) {			/* We'll generate a redirect */
													ICP_reply(&clientAddr, clientAddrLen, soc, ICP_OP_HIT, Version, 0, Request_Number, 0, 0, 0, UrlRequest);
													LogReply = "ICP_OP_HIT";
													free(UrlRedirect);
													break;
												}
												free(UrlRedirect);
											}
										}
										/* We won't retrive the cache MISS online, no way! */
										ICP_reply(&clientAddr, clientAddrLen, soc, ICP_OP_MISS_NOFETCH, Version, 0, Request_Number, 0, 0, 0, UrlRequest);
										LogReply = "ICP_OP_MISS_NOFETCH";
									}
								}
								break;
							case ICP_OP_SECHO:
								{
									UrlRequest = &Payload[4];
									LogRequest = "ICP_OP_QUERY";
									LogReply = "ICP_OP_QUERY";
									ICP_reply(&clientAddr, clientAddrLen, soc, ICP_OP_SECHO, Version, 0, Request_Number, 0, 0, 0, UrlRequest);
								}
								break;
							default:
								LogRequest = "NOTIMPLEMENTED";
								LogReply = "ICP_OP_ERR";
								ICP_reply(&clientAddr, clientAddrLen, soc, ICP_OP_ERR, Version, 0, Request_Number, 0, 0, 0, NULL);
								break;
							}
						} else {
							ICP_reply(&clientAddr, clientAddrLen, soc, ICP_OP_ERR, 2, 0, Request_Number, 0, 0, 0, NULL);
						}
					} /* Ignored (RFC2186) */
				} else {
					ICP_reply(&clientAddr, clientAddrLen, soc, ICP_OP_ERR, Version, 0, Request_Number, 0, 0, 0, NULL);
				}
			}

			/* Logging */
			{
				String ip = STRING_EMPTY;
				SOCaddr serverClient;
				int lenServerClient = (int) sizeof(serverClient);
				SOCaddr_copyaddr(serverClient, lenServerClient, &clientAddr, clientAddrLen);
				if (lenServerClient > 0) {
					ip = getip(&serverClient, lenServerClient);
				} else {
					StringCopy(ip, "unknown");
				}
				LOG("ICP %s %s/%s %s" _ StringBuff(ip) _ LogRequest _ LogReply _ (UrlRequest ? UrlRequest : "-") );
				StringFree(ip);
			}

		}
	}
	if (soc != INVALID_SOCKET) {
#ifdef _WIN32
		closesocket(soc);
#else
		close(soc);
#endif
	}
	free(buffer);
	return 1;
}

static int proxytrack_start(PT_Indexes indexes, T_SOC soc, T_SOC socICP) {
	int ret = 1;
	if (proxytrack_process_generic(proxytrack_start_ICP, indexes, socICP)) {
		//if (!proxytrack_process_generic(proxytrack_start_HTTP, indexes, soc))
		if (!proxytrack_start_HTTP(indexes, soc)) {
			ret = 0;
		}
	} else {
		ret = 0;
	}
	return ret;
}

